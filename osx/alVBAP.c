/*
 * Bits of code were inspired by, and cut-and-pasted from Ville Pulkki's Vector
 *  Base Amplitude Panning (VBAP) source code. Those algorithms enable us to
 *  efficiently spatialize audio sources across multiple speakers. Here is the
 *  copyright that code falls under:
 *
 * ---
 * Copyright 1998 by Ville Pulkki, Helsinki University of Technology.  All
 * rights reserved.
 *
 * The software may be used, distributed, and included to commercial
 * products without any charges. When included to a commercial product,
 * the method "Vector Base Amplitude Panning" and its developer Ville
 * Pulkki must be referred to in documentation.
 *
 * This software is provided "as is", and Ville Pulkki or Helsinki
 * University of Technology make no representations or warranties,
 * expressed or implied. By way of example, but not limitation, Helsinki
 * University of Technology or Ville Pulkki make no representations or
 * warranties of merchantability or fitness for any particular purpose or
 * that the use of the licensed software or documentation will not
 * infringe any third party patents, copyrights, trademarks or other
 * rights. The name of Ville Pulkki or Helsinki University of Technology
 * may not be used in advertising or publicity pertaining to distribution
 * of the software.
 * ---
 *
 * Set "USE_VBAP" to "false" in the makefile to disable code that falls under
 * this copyright.
 */

#if !USE_VBAP
#error Please set USE_VBAP to true in the makefile.
#endif

#include "alInternal.h"

static inline ALboolean __alCalc2DInvTmatrix(ALfloat azi1,
                                             ALfloat azi2,
                                             ALfloat *inv_mat)
{
    register ALfloat det;
    ALfloat x1,x2,x3,x4;

    __alSinCos(azi1, &x2, &x1);
    __alSinCos(azi2, &x4, &x3);

    det = (x1 * x4) - (x3 * x2);
    if (fabsf(det) <= 0.001f)
        return AL_FALSE;

    // reciprocal so we don't have to stall the pipeline with four divides.
    det = __alRecipEstimate(det);

    inv_mat[0] = (x4 * det);   // !!! FIXME: Altivec this...?
    inv_mat[1] = (-x3 * det);
    inv_mat[2] = (-x2 * det);
    inv_mat[3] = (x1 * det);
    return AL_TRUE;
} // __alCalc2DInvTmatrix


// "Triangulate" is a bit of a misnomer...we're picking speaker pairs, not
//  groups of three when all the speakers are on the same plane.
static ALvoid __alcTriangulateSpeakersVBAP2D(ALdevice *device)
{
    register ALsizei max = device->speakers;
    register ALfloat tmp;
    register ALsizei i;
    register ALsizei j;
    register ALsizei index = 0;
    register ALfloat azimuth;
    register ALfloat azi1;
    register ALfloat azi2;
    register ALsizei amount = 0;
    register ALsizei *sortedptr = device->vbapsorted;
    register ALfloat *matrixptr = device->vbapmatrix;
    const ALfloat ang2rad = 2.0f * 3.141592f / 360.0f;
    const ALfloat adjacent = (3.1415927f - 0.175f);
    ALfloat azimuths[AL_MAXSPEAKERS];
    ALsizei sorted[AL_MAXSPEAKERS];

    for (i = 0; i < max; i++)  // !!! FIXME: Optimize.
    {
        register ALfloat azi = device->speakerazimuths[i];
        register ALfloat x = cosf(azi * ang2rad);
        register ALfloat y = sinf(azi * ang2rad);
        register ALfloat absy = fabsf(y);

        azi = acosf(x);
        if (absy > 0.001f)
        	azi *= y / absy;
        azimuths[i] = azi;

        device->speakerpos[(i*3)+0] = x;
        device->speakerpos[(i*3)+1] = y;
        device->speakerpos[(i*3)+2] = 0.0f;  // z.
    } // for

    // sort speakers by azimuth.
    for (i = 0; i < max; i++)  // !!! FIXME: Better sort algorithm!
    {
        tmp = 2000.0f;
        for (j = 0; j < max; j++)
        {
            azimuth = azimuths[j];
            if (azimuth <= tmp)
            {
                tmp = azimuth;
                index = j;
            } // if
        } // for

        sorted[i] = index;
        azimuths[index] += 4000.0f;
    } // for

    for (i = 0; i < max; i++)  // !!! FIXME: yuck.
        azimuths[i] -= 4000.0f;

    // okay, we're sorted. Find adjacent speakers...
    for (i = 0; i < max-1; i++)
    {
        azi1 = azimuths[sorted[i+1]];
        azi2 = azimuths[sorted[i]];
        if ((azi1 - azi2) <= adjacent)
        {
            if (__alCalc2DInvTmatrix(azi2, azi1, matrixptr))
            {
                sortedptr[0] = sorted[i];
                sortedptr[1] = sorted[i+1];
                sortedptr += 2;
                matrixptr += 4;
                amount++;
            } // if
        } // if
    } // for

    // handle wraparound to start of circle...
    azi1 = azimuths[sorted[max-1]];
    azi2 = azimuths[sorted[0]];
    if ((6.283f - azi1 + azi2) <= adjacent)
    {
        if (__alCalc2DInvTmatrix(azi1, azi2, matrixptr))
        {                                               
            sortedptr[0] = sorted[max-1];
            sortedptr[1] = sorted[0];
            amount++;
        } // if
    } // if

    #if 1
    matrixptr = device->vbapmatrix;
    sortedptr = device->vbapsorted;
    printf("VBAP2D triangulation:\n");
    for (i = 0; i < amount; i++)
    {
        printf("  pair %d: %d %d ... matrix: %f %f %f %f\n", i,
                sortedptr[0], sortedptr[1], matrixptr[0], matrixptr[1],
                matrixptr[2], matrixptr[3]);
        sortedptr += 2;
        matrixptr += 4;
    } // for
    #endif

    device->vbapgroups = amount;  // move from register to structure.
} // __alcTriangulateSpeakersVBAP2D


static ALvoid __alcTriangulateSpeakersVBAP3D(ALdevice *device)
{
    assert(0);  // !!! FIXME: Write me!
} // __alcTriangulateSpeakersVBAP3D


ALvoid __alcTriangulateSpeakers(ALdevice *device)
{
    // decide on speaker configuration...
    if (device->speakers == 2)
    {
        // !!! FIXME: Use VBAP2D if speakers aren't roughly in stdstereo
        // !!! FIXME:  positions (and/or introduce a SPKCFG_REVERSE_STEREO).

        // no VBAP, just stereo panning...
        device->speakerConfig = SPKCFG_STDSTEREO;
    } // if
    else
    {
        #if 0  // !!! FIXME: Implement 3D triangulation before using this.
        register ALfloat *pos = device->speakerpos + 1; // align to Y coord.
        register ALsizei max = device->speakers;
        register ALsizei i;

        for (i = 0; i < max; i++, pos += 3)
        {
            if (!EPSILON_EQUAL(*pos, 0.0f))  // elevated...must be 3D.
            {
                device->speakerConfig = SPKCFG_VBAP_3D;
                break;
            } // if
        } // for

        if (i == max)  // all speakers on plane with physical listener?
            device->speakerConfig = SPKCFG_VBAP_2D;  // fast path.

        #elif 0
        device->speakerConfig = SPKCFG_VBAP_2D;  // fast path.
        #else
        device->speakerConfig = SPKCFG_POSATTENUATION;  // fast path.
        #endif
    } // else

    // !!! FIXME: convert speaker positions to azimuth/elevation!
    #error this is broken without azi/ele conversion!
    assert(0);

    // Group speakers into pairs (2D) or triangles (3D) based on position.
    switch (device->speakerConfig)
    {
        case SPKCFG_STDSTEREO:
        case SPKCFG_POSATTENUATION:
            break;  // nothing to do here in this case.

        case SPKCFG_VBAP_2D:
            __alcTriangulateSpeakersVBAP2D(device);
            break;

        case SPKCFG_VBAP_3D:
            __alcTriangulateSpeakersVBAP3D(device);
            break;

        default:
            assert(0);
            break;
    } // switch
} // __alcTriangulateSpeakers


static inline ALvoid __alDoVBAP2D(ALcontext *ctx, ALsource *src,
                                  ALvector pos, ALfloat gain)
{
    register ALdevice *device = ctx->device;
    register ALfloat srcx = -pos[0];
    register ALfloat srcz = pos[2];
    register ALfloat loudest = -0.01f;
    register ALsizei max = device->vbapgroups;
    register ALfloat *mtx = device->vbapmatrix;
    register ALsizei pair = -1;
    register ALfloat g0 = 0.0f;
    register ALfloat g1 = 0.0f;
    register ALsizei ls0;
    register ALsizei ls1;
    register ALsizei i;
    register ALfloat power;

    // !!! FIXME: This probably isn't right.
    if ( (EPSILON_EQUAL(srcx, 0.0f)) && (EPSILON_EQUAL(srcz, 0.0f)) )
        srcz = 0.00001f;

    for (i = 0; i < max; i++, mtx += 4)
    {
        register ALfloat gtmp0 = (srcz * mtx[0]) + (srcx * mtx[1]);
        register ALfloat gtmp1 = (srcz * mtx[2]) + (srcx * mtx[3]);
        register ALfloat quieter = (gtmp0 < gtmp1) ? gtmp0 : gtmp1;
        if (quieter > loudest)
        {
            loudest = quieter;
            g0 = gtmp0;
            g1 = gtmp1;
            pair = i;
        } // if
    } // if

    assert(pair != -1);  // make sure we actually chose a speaker pair...
    pair *= 2;
    ls0 = device->vbapsorted[pair];
    ls1 = device->vbapsorted[pair+1];

    // !!! FIXME: this could be optimized with an __alRecipSqrt().
    power = __alRecipEstimate(__alSqrt((g0 * g0) + (g1 * g1)));

    // VBAP positional gain is done...now apply master gains and store to RAM.
    power *= gain;
    g0 = g0 * device->speakergains[ls0] * power;
    g1 = g1 * device->speakergains[ls1] * power;
    CLAMP(g0, 0.0f, 1.0f);
    CLAMP(g1, 0.0f, 1.0f);
    src->channelGain0 = g0;
    src->channelGain1 = g1;
    src->channel0 = ls0;
    src->channel1 = ls1;
} // __alDoVBAP2D


static inline ALvoid __alDoVBAP3D(ALcontext *ctx, ALsource *src,
                                  ALvector pos, ALfloat gain)
{
    register ALdevice *device = ctx->device;
    register ALfloat srcx = pos[0];
    register ALfloat srcy = pos[1];
    register ALfloat srcz = pos[2];
    register ALfloat loudest = -0.01f;
    register ALsizei max = device->vbapgroups;
    register ALfloat *mtx = device->vbapmatrix;
    register ALsizei triplet = -1;
    register ALfloat g0 = 0.0f;
    register ALfloat g1 = 0.0f;
    register ALfloat g2 = 0.0f;
    register ALsizei ls0;
    register ALsizei ls1;
    register ALsizei ls2;
    register ALsizei i;
    register ALfloat power;

    for (i = 0; i < max; i++, mtx += 9)  // !!! FIXME: Altivec?
    {
        register ALfloat gtmp0 = (srcx*mtx[0]) + (srcy*mtx[1]) + (srcz*mtx[2]);
        register ALfloat gtmp1 = (srcx*mtx[3]) + (srcy*mtx[4]) + (srcz*mtx[5]);
        register ALfloat gtmp2 = (srcx*mtx[6]) + (srcy*mtx[7]) + (srcz*mtx[8]);
        register ALfloat quietest = (gtmp0 < gtmp1) ? gtmp0 : gtmp1;

        if (gtmp2 < quietest)
            quietest = gtmp2;

        if (quietest > loudest) // quietest of group is still loudest overall?
        {
            loudest = quietest;
            g0 = gtmp0;
            g1 = gtmp1;
            g2 = gtmp2;
            triplet = i;
        } // if
    } // if

    assert(triplet != -1);  // make sure we actually chose a speaker triplet...
    ls0 = device->vbapsorted[triplet];
    ls1 = device->vbapsorted[triplet+1];
    ls2 = device->vbapsorted[triplet+2];

    // !!! FIXME: this could be optimized with an __alRecipSqrt().
    power = __alRecipEstimate(__alSqrt((g0 * g0) + (g1 * g1) + (g2 * g2)));

    // VBAP positional gain is done...now apply master gains and store to RAM.
    power *= gain;
    g0 = g0 * device->speakergains[ls0] * power;
    g1 = g1 * device->speakergains[ls1] * power;
    g2 = g2 * device->speakergains[ls2] * power;
    CLAMP(g0, 0.0f, 1.0f);
    CLAMP(g1, 0.0f, 1.0f);
    CLAMP(g2, 0.0f, 1.0f);
    src->channelGain0 = g0;
    src->channelGain1 = g1;
    src->channelGain2 = g2;
    src->channel0 = ls0;
    src->channel1 = ls1;
    src->channel2 = ls2;
} // __alDoVBAP3D


ALvoid __alDoVBAP(ALcontext *ctx, ALsource *src, ALvector pos, ALfloat gain)
{
    assert( (ctx->device->speakerConfig == SPKCFG_VBAP_2D) ||
            (ctx->device->speakerConfig == SPKCFG_VBAP_3D) );

    if (ctx->device->speakerConfig == SPKCFG_VBAP_2D)
        __alDoVBAP2D(ctx, src, pos, gain);
    else
        __alDoVBAP3D(ctx, src, pos, gain);
} // __alDoVBAP

// end of alVBAP.c ...

