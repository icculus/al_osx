/**
 * OpenAL cross platform audio library
 * Copyright (C) 1999-2000 by authors.
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 */

#include "alInternal.h"


#if HAVE_PRAGMA_EXPORT
#pragma export off
#endif

// !!! FIXME: altivec this whole file. --ryan.


#define FORCE_STD_SQRT 0

// Square root stuff. Use faster, single precision versions.
#if ((MACOSX) && (!FORCE_STD_SQRT))

static Float32 __alSqrtUnknown(Float32 num);
static Float32 (*__alSqrt)(Float32 num) = __alSqrtUnknown;


static Float32 __alSqrtOpcode(Float32 num)
{
    register Float32 retval;

    __asm__ __volatile__ (
        "fsqrts %0, %1  \n\t"
            : "=f" (retval) : "f" (num)
    );

    return(retval);
} // __alSqrtOpcode


// Compliments of Jeffrey Lim, in regards to something completely unrelated.
//  Says him:
//    "I did some testing, and the fsqrte is very inaccurate, as you said -
//     but after 2 iterations of newton-raphson, it seems to be accurate to 1
//     ULP of the answer provided by sqrt()".
static Float32 __alSqrtRecipAndNewtonRaphson(Float32 num)
{
    register Float32 t1;
    register Float32 t2;
    register Float32 t3;
    register Float32 hx;

    if (num == 0.0f)
        return(0.0f);

    __asm__ __volatile__ (
        "frsqrte %0, %1  \n\t"
            : "=f" (t1) : "f" (num)
    );

    hx = num * 0.5f;
    t2 = t1 * (1.5f - hx * t1 * t1);
    t3 = t2 * (1.5f - hx * t2 * t2);

    return(t3 * num);
} // __alSqrtRecipAndNewtonRaphson



// !!! FIXME: "6" is really gestaltPowerPCHas64BitSupport ... for now, this
// !!! FIXME:  signifies all Macs with "fsqrts" ... the G5.
#define __AL_GESTALT_HAS_FSQRTS 6

// first call determines which sqrt() to use, after that it calls it directly.
static Float32 __alSqrtUnknown(Float32 num)
{
    ALboolean hasFsqrtsOpcode = AL_FALSE;
    long cpufeature = 0;
    OSErr err = Gestalt(gestaltPowerPCProcessorFeatures, &cpufeature);
    if (err == noErr)
    {
        if ((1 << __AL_GESTALT_HAS_FSQRTS) & cpufeature)
            hasFsqrtsOpcode = AL_TRUE;
    } // if

    if (hasFsqrtsOpcode == AL_TRUE)
        __alSqrt = __alSqrtOpcode;
    else
        __alSqrt = __alSqrtRecipAndNewtonRaphson;

    return(__alSqrt(num));
} // __alSqrtUnknown

#else
#define __alSqrt(x) ((ALfloat) sqrt(x))  // portable and precise, but slower.
#endif

ALAPI ALvoid ALAPIENTRY alCrossproduct(ALfloat vector1[3],ALfloat vector2[3],ALfloat vector3[3])
{
	vector3[0]=(vector1[1]*vector2[2]-vector1[2]*vector2[1]);
	vector3[1]=(vector1[2]*vector2[0]-vector1[0]*vector2[2]);
	vector3[2]=(vector1[0]*vector2[1]-vector1[1]*vector2[0]);
}

ALAPI ALfloat ALAPIENTRY alDotproduct(ALfloat vector1[3],ALfloat vector2[3])
{
	return (vector1[0]*vector2[0]+vector1[1]*vector2[1]+vector1[2]*vector2[2]);
}

ALAPI ALvoid ALAPIENTRY alNormalize(ALfloat vector[3])
{
	ALfloat length;

	length=(float)__alSqrt(alDotproduct(vector,vector));
	vector[0]/=length;
	vector[1]/=length;
	vector[2]/=length;
}

ALAPI ALvoid ALAPIENTRY alMatrixVector(ALfloat matrix[3][3],ALfloat vector[3])
{
	ALfloat result[3];

	result[0]=matrix[0][0]*vector[0]+matrix[0][1]*vector[1]+matrix[0][2]*vector[2];
	result[1]=matrix[1][0]*vector[0]+matrix[1][1]*vector[1]+matrix[1][2]*vector[2];
	result[2]=matrix[2][0]*vector[0]+matrix[2][1]*vector[1]+matrix[2][2]*vector[2];
	//BlockMove(result, vector, sizeof(result));
	memcpy(vector, result, sizeof(result));
}


static ALvoid __alCalculateSourceParameters(ALcontext *ctx, ALsource *src, ALfloat *Pitch, ALfloat *Panning, ALfloat *Volume)
{
    ALfloat Position[3],Velocity[3],Distance;
    ALfloat SourceListenerDir[3];
	ALfloat U[3],V[3],N[3];
	ALfloat Matrix[3][3];
	ALfloat minGain, maxGain;
    ALfloat CalcPitch = src->pitch;
    ALfloat CalcVolume = src->gain;
    ALenum distanceModel = ctx->distanceModel;
    ALlistener *listener = &ctx->listener;

	// Convert source position to listener's coordinate system
    minGain = src->minGain;
    maxGain = src->maxGain;
    Position[0] = src->Position[0];
    Position[1] = src->Position[1];
    Position[2] = src->Position[2];
    Velocity[0] = src->Velocity[0];
    Velocity[1] = src->Velocity[1];
    Velocity[2] = src->Velocity[2];

	// Translate Listener to origin if not in AL_SOURCE_RELATIVE mode
	if (src->srcRelative == AL_FALSE)
	{
        Position[0]-=listener->Position[0];
        Position[1]-=listener->Position[1];
        Position[2]-=listener->Position[2];
    }

    // Set SourceListenerDir to be used later for doppler
    SourceListenerDir[0] = Position[0];
    SourceListenerDir[1] = Position[1];
    SourceListenerDir[2] = Position[2];

    // Align coordinate system axis
    alCrossproduct(listener->Up,listener->Forward,U);
    alNormalize(U);
    alCrossproduct(listener->Forward,U,V);
    alNormalize(V);
    //BlockMove(listener->Forward, N, sizeof(N));
    memcpy(N, listener->Forward, sizeof(N));
    alNormalize(N);
    Matrix[0][0]=U[0]; Matrix[0][1]=V[0]; Matrix[0][2]=N[0];
    Matrix[1][0]=U[1]; Matrix[1][1]=V[1]; Matrix[1][2]=N[1];
    Matrix[2][0]=U[2]; Matrix[2][1]=V[2]; Matrix[2][2]=N[2];
    alMatrixVector(Matrix,Position);
    // Convert into falloff and panning
    Distance=(float)__alSqrt(Position[0]*Position[0]+Position[1]*Position[1]+Position[2]*Position[2]); // * ctx->distanceScale;

    // !!! FIXME: Should we even be checking channel numbers here (should only
    // !!! FIXME:  be called for mono buffers...
    //if ((Distance != 0.0f) && (ctx->buffers[src->srcBufferNum-1].channels == 1))
    if (Distance != 0.0f)
        *Panning=(ALfloat)(0.5+0.5*0.707*cos(atan2(Position[2],Position[0])));
    else
        *Panning=0.5f;
			
    if ((distanceModel!= AL_NONE) && (Distance > 0))
    {
        if (distanceModel==AL_INVERSE_DISTANCE_CLAMPED)
        {
            if (Distance < src->referenceDistance)
                Distance = src->referenceDistance;
            if (Distance > src->maxDistance)
                Distance = src->maxDistance;
        }
        CalcVolume=(src->gain*listener->Gain*src->referenceDistance)/(src->referenceDistance+src->rolloffFactor*(Distance-src->referenceDistance));
    }
    else
    {
        CalcVolume=(src->gain*listener->Gain);
    }

    // cap Volume by min/max gains
    if (CalcVolume < minGain)
        CalcVolume = minGain;
    else if (CalcVolume > maxGain)
        CalcVolume = maxGain;
			
    // Calculate doppler
    if (!((SourceListenerDir[0]==0)&&(SourceListenerDir[1]==0)&&(SourceListenerDir[2]==0)))
    {
        ALfloat dopplerVelocity = ctx->dopplerVelocity;
        ALfloat ListenerSpeed, SourceSpeed;
        alNormalize(SourceListenerDir);
        ListenerSpeed = -alDotproduct(SourceListenerDir, listener->Velocity);
        SourceSpeed = alDotproduct(SourceListenerDir, src->Velocity);
			
        CalcPitch=CalcPitch+CalcPitch*ctx->dopplerFactor*((dopplerVelocity-ListenerSpeed)/(dopplerVelocity+SourceSpeed)-1);
        if (CalcPitch < 0.5f)
            CalcPitch = 0.5f;
        else if (CalcPitch > 2.0f)
            CalcPitch = 2.0f;
    }

    if (CalcVolume > 1.0f)
        CalcVolume = 1.0f;

    *Volume = CalcVolume;
    *Pitch = CalcPitch;
}


// Mixing...

#define DIVBY32767 0.000030519f
#define DIVBY127   0.007874016f

#define MINVOL 0.005f
#define FULL_VOLUME 0.512f


ALvoid __alRecalcMonoSource(ALcontext *ctx, ALsource *src)
{
    // Some source state has changed? Recalculate what's needed.
    if (src->needsRecalculation == AL_TRUE)
    {
        ALfloat Pitch;
        ALfloat Panning;
        ALfloat Volume;
        register ALfloat MonoGain;
        register ALfloat Panningx2;

        src->needsRecalculation = AL_FALSE;
        __alCalculateSourceParameters(ctx, src, &Pitch, &Panning, &Volume);
    	Panningx2 = Panning * 2.0f;
    	MonoGain = FULL_VOLUME * Volume;
	    src->CalcGainLeft = MonoGain * Panningx2;
	    src->CalcGainRight = MonoGain * (2.0f - Panningx2);
    } // if
} // __alRecalcMonoSource


// !!! FIXME: Make these mixers not suck.
// !!! FIXME:  ... and do altivec versions of them, too.

UInt32 __alMixMono8(ALcontext *ctx, ALbuffer *buf, ALsource *src, Float32 *_dst, UInt32 frames)
{
    register UInt32 buflen = buf->allocatedSpace - src->bufferReadIndex;
    register UInt32 channels;
    register ALsizei overflow = 0;
    register SInt8 *in;
    register Float32 *dst;
    register Float32 *max;
    register Float32 gainLeft;
    register Float32 gainRight;
    register Float32 sample;

    __alRecalcMonoSource(ctx, src);
	gainLeft = src->CalcGainLeft;
	gainRight = src->CalcGainRight;

    overflow = frames - buflen;
    if (overflow < 0)
        overflow = 0;
    else
        frames = buflen;

    // Too quiet to be worth playing? Just skip ahead in the buffer appropriately.
    if ((gainLeft > MINVOL) || (gainRight > MINVOL))
    {
        if (gainLeft > 1.0f)
            gainLeft = 1.0f;

        if (gainRight > 1.0f)
            gainRight = 1.0f;

        // Mix to the output buffer...
        channels = ctx->device->streamFormat.mChannelsPerFrame;
        dst = _dst;
        max = dst + (frames * channels);
        in = ((SInt8 *) buf->mixData) + src->bufferReadIndex;

        gainLeft *= DIVBY127;
        gainRight *= DIVBY127;
        while (dst < max)
        {
            sample = ((Float32) *in);
            in++;
            dst[0] += sample * gainLeft;
            dst[1] += sample * gainRight;
            dst += channels;  // skip rest of channels.
        } // while
    } // if

    src->bufferReadIndex += frames;
    return(overflow);
} // __alMixMono8


UInt32 __alMixMono16(ALcontext *ctx, ALbuffer *buf, ALsource *src, Float32 *_dst, UInt32 frames)
{
    register UInt32 buflen = (buf->allocatedSpace >> 1) - src->bufferReadIndex;
    register UInt32 channels;
    register ALsizei overflow = 0;
    register SInt16 *in;
    register Float32 *dst;
    register Float32 *max;
    register Float32 gainLeft;
    register Float32 gainRight;
    register Float32 sample;

    __alRecalcMonoSource(ctx, src);
    gainLeft = src->CalcGainLeft;
    gainRight = src->CalcGainRight;

    overflow = frames - buflen;
    if (overflow < 0)
        overflow = 0;
    else
        frames = buflen;

    // Too quiet to be worth playing? Just skip ahead in the buffer appropriately.
    if ((gainLeft > MINVOL) || (gainRight > MINVOL))
    {
        if (gainLeft > 1.0f)
            gainLeft = 1.0f;

        if (gainRight > 1.0f)
            gainRight = 1.0f;

        // Mix to the output buffer...
        channels = ctx->device->streamFormat.mChannelsPerFrame;
        dst = _dst;
        max = dst + (frames * channels);
        in = ((SInt16 *) buf->mixData) + src->bufferReadIndex;

        gainLeft *= DIVBY32767;
        gainRight *= DIVBY32767;
        while (dst < max)
        {
            sample = ((Float32) *in);
            in++;
            dst[0] += sample * gainLeft;
            dst[1] += sample * gainRight;
            dst += channels;  // skip rest of channels.
        } // while
    } // if

    src->bufferReadIndex += frames;
    return(overflow);
} // __alMixMono16


UInt32 __alMixStereo8(ALcontext *ctx, ALbuffer *buf, ALsource *src, Float32 *_dst, UInt32 frames)
{
    register UInt32 buflen = (buf->allocatedSpace >> 1) - src->bufferReadIndex;
    register UInt32 channels;
    register ALsizei overflow = 0;
    register SInt8 *in;
    register Float32 *dst;
    register Float32 *max;
    register Float32 gain;

	gain = src->gain * ctx->listener.Gain;

    overflow = frames - buflen;
    if (overflow < 0)
        overflow = 0;
    else
        frames = buflen;

    // Too quiet to be worth playing? Just skip ahead in the buffer appropriately.
    if (gain > MINVOL)
    {
    	if (gain > 1.0f)
            gain = 1.0f;

        // Mix to the output buffer...
        channels = ctx->device->streamFormat.mChannelsPerFrame;
        dst = _dst;
        max = dst + (frames * channels);
        in = ((SInt8 *) buf->mixData) + (src->bufferReadIndex << 1);

        gain *= DIVBY127;
        while (dst < max)
        {
            dst[0] += ((Float32) in[0]) * gain;
            dst[1] += ((Float32) in[1]) * gain;
            in += 2;
            dst += channels;  // skip rest of channels.
        } // while
    } // if

    src->bufferReadIndex += frames;
    return(overflow);
} // __alMixStereo8


UInt32 __alMixStereo16(ALcontext *ctx, ALbuffer *buf, ALsource *src, Float32 *_dst, UInt32 frames)
{
    register UInt32 buflen = (buf->allocatedSpace >> 2) - src->bufferReadIndex;
    register UInt32 channels;
    register ALsizei overflow = 0;
    register SInt16 *in;
    register Float32 *dst;
    register Float32 *max;
    register Float32 gain;

	gain = src->gain * ctx->listener.Gain;

    overflow = frames - buflen;
    if (overflow < 0)
        overflow = 0;
    else
        frames = buflen;

    // Too quiet to be worth playing? Just skip ahead in the buffer appropriately.
    if (gain > MINVOL)
    {
    	if (gain > 1.0f)
            gain = 1.0f;

        // Mix to the output buffer...
        channels = ctx->device->streamFormat.mChannelsPerFrame;
        dst = _dst;
        max = dst + (frames * channels);
        in = ((SInt16 *) buf->mixData) + (src->bufferReadIndex << 1);

        gain *= DIVBY32767;
        while (dst < max)
        {
            dst[0] += ((Float32) in[0]) * gain;
            dst[1] += ((Float32) in[1]) * gain;
            in += 2;
            dst += channels;  // skip rest of channels.
        } // while
    } // if

    src->bufferReadIndex += frames;
    return(overflow);
} // __alMixStereo16


UInt32 __alMixMono8_altivec(ALcontext *ctx, ALbuffer *buf, ALsource *src, Float32 *_dst, UInt32 frames)
{
    register UInt32 buflen = buf->allocatedSpace - src->bufferReadIndex;
    register ALsizei overflow = 0;
    register SInt8 *in = ((SInt8 *) buf->mixData) + src->bufferReadIndex;
    register Float32 *dst = _dst;
    register Float32 *max;
    register Float32 gainLeft;
    register Float32 gainRight;
    register Float32 sample;
    register size_t dsti = ((size_t) dst) & 0x0000000F;

    assert(ctx->device->streamFormat.mChannelsPerFrame == 2);

    __alRecalcMonoSource(ctx, src);
    gainLeft = src->CalcGainLeft;
    gainRight = src->CalcGainRight;

    if (dsti)  // Destination isn't 16-byte aligned?
    {
        if ((dsti % 2) && (frames > dsti))  // bump dst to 16-byte alignment.
        {
            register Float32 convGainLeft = gainLeft * DIVBY127;
            register Float32 convGainRight = gainRight * DIVBY127;
            while (UNALIGNED_PTR(dst))
            {
                sample = ((Float32) *in);
                in++;
                dst[0] += sample * convGainLeft;
                dst[1] += sample * convGainRight;
                dst += 2;
                frames--;
            } // while
        } // if

        else  // dst will never align in this run; use scalar version.  :(
        {
            return(__alMixMono8(ctx, buf, src, dst, frames));
        } // else
    } // if

    overflow = frames - buflen;
    if (overflow < 0)
        overflow = 0;
    else
        frames = buflen;

    // Too quiet to be worth playing? Just skip ahead in the buffer appropriately.
    if ((gainLeft > MINVOL) || (gainRight > MINVOL))
    {
        // Permute vectors to convert mono channel to stereo.
        register vector unsigned char permute1 = (vector unsigned char)
                                                    ( 0x00, 0x00, 0x01, 0x01,
                                                      0x02, 0x02, 0x03, 0x03,
                                                      0x04, 0x04, 0x05, 0x05,
                                                      0x06, 0x06, 0x07, 0x07 );
        register vector unsigned char permute2;
        register vector unsigned char aligner = vec_lvsl(0, in);

        register vector float gainvec = (vector float) ( DIVBY127, DIVBY127,
                                                         DIVBY127, DIVBY127 );
        register vector float zero = (vector float) vec_splat_u32(0);
        register vector float mixvec1;
        register vector float mixvec2;
        register vector float mixvec3;
        register vector float mixvec4;
        register vector float mixvec5;
        register vector float mixvec6;
        register vector float mixvec7;
        register vector float mixvec8;
        register vector float v1;
        register vector float v2;
        register vector float v3;
        register vector float v4;
        register vector float v5;
        register vector float v6;
        register vector float v7;
        register vector float v8;
        register vector signed short conv_short;
        register vector signed int conv_int;
        register vector signed char ld;
        register vector signed char ld2;
        register vector signed char ld_overflow;
        register int extra = (frames % 32);

        union
        {
            vector float v;
            float f[4];
        } gaincvt;

        permute2 = vec_add(permute1, vec_splat_u8(8));

        if (gainLeft > 1.0f)
            gainLeft = 1.0f;

        if (gainRight > 1.0f)
            gainRight = 1.0f;

        gaincvt.f[0] = gaincvt.f[2] = gainLeft;
        gaincvt.f[1] = gaincvt.f[3] = gainRight;
        gainvec = vec_madd(gainvec, gaincvt.v, zero);

        // Mix to the output buffer...
        frames -= extra;
        max = dst + (frames << 1);

        ld = vec_ld(0, in);
        while (dst < max)
        {
            // Get the sample points into a vector register...
            ld_overflow = vec_ld(16, in);  // read 16 8-bit samples.

            in += 16;

            mixvec1 = vec_ld(0x00, dst);
            mixvec2 = vec_ld(0x10, dst);
            mixvec3 = vec_ld(0x20, dst);
            mixvec4 = vec_ld(0x30, dst);
            mixvec5 = vec_ld(0x40, dst);
            mixvec6 = vec_ld(0x50, dst);
            mixvec7 = vec_ld(0x60, dst);
            mixvec8 = vec_ld(0x70, dst);

            ld = vec_perm(ld, ld_overflow, aligner);

            // convert to stereo, so there's 8 stereo samples per vector.
            // ld  == 0l 0r 1l 1r 2l 2r 3l 3r 4l 4r 5l 5r 6l 6r 7l 7r
            // ld2 == 8l 8r 9l 9r Al Ar Bl Br Cl Cr Dl Dr El Er Fl Fr
            ld2 = vec_perm(ld, ld, permute2);
            ld = vec_perm(ld, ld, permute1);

            // convert to 16-bit ints, then 32-bit ints, then floats...
            conv_short = vec_unpackh(ld);
            conv_int = vec_unpackh(conv_short);
            v1 = vec_ctf(conv_int, 0);
            conv_int = vec_unpackl(conv_short);
            v2 = vec_ctf(conv_int, 0);
            conv_short = vec_unpackl(ld);
            conv_int = vec_unpackh(conv_short);
            v3 = vec_ctf(conv_int, 0);
            conv_int = vec_unpackl(conv_short);
            v4 = vec_ctf(conv_int, 0);
            conv_short = vec_unpackh(ld2);
            conv_int = vec_unpackh(conv_short);
            v5 = vec_ctf(conv_int, 0);
            conv_int = vec_unpackl(conv_short);
            v6 = vec_ctf(conv_int, 0);
            conv_short = vec_unpackl(ld2);
            conv_int = vec_unpackh(conv_short);
            v7 = vec_ctf(conv_int, 0);
            conv_int = vec_unpackl(conv_short);
            v8 = vec_ctf(conv_int, 0);

            // change to float scale, apply gain, and mix with dst...
            v1 = vec_madd(v1, gainvec, mixvec1);
            v2 = vec_madd(v2, gainvec, mixvec2);
            v3 = vec_madd(v3, gainvec, mixvec3);
            v4 = vec_madd(v4, gainvec, mixvec4);
            v5 = vec_madd(v5, gainvec, mixvec5);
            v6 = vec_madd(v6, gainvec, mixvec6);
            v7 = vec_madd(v7, gainvec, mixvec7);
            v8 = vec_madd(v8, gainvec, mixvec8);

            // store converted version back to RAM...
            vec_st(v1, 0x00, dst);
            vec_st(v2, 0x10, dst);
            vec_st(v3, 0x20, dst);
            vec_st(v4, 0x30, dst);
            vec_st(v5, 0x40, dst);
            vec_st(v6, 0x50, dst);
            vec_st(v7, 0x60, dst);
            vec_st(v8, 0x70, dst);

            dst += 32;
            ld = ld_overflow;
        } // while

        // Handle overflow as scalar data.
        if (extra)
        {
            gainLeft *= DIVBY127;
            gainRight *= DIVBY127;
            max += extra;
            frames += extra;
            while (dst < max)
            {
                sample = ((Float32) *in);
                in++;
                dst[0] += sample * gainLeft;
                dst[1] += sample * gainRight;
                dst += 2;
            } // while
        } // if
    } // if

    src->bufferReadIndex += frames;
    return(overflow);
} // __alMixMono8_altivec


UInt32 __alMixMono16_altivec(ALcontext *ctx, ALbuffer *buf, ALsource *src, Float32 *_dst, UInt32 frames)
{
    register UInt32 buflen = (buf->allocatedSpace >> 1) - src->bufferReadIndex;
    register ALsizei overflow = 0;
    register SInt16 *in = ((SInt16 *) buf->mixData) + src->bufferReadIndex;
    register Float32 *dst = _dst;
    register Float32 *max;
    register Float32 gainLeft;
    register Float32 gainRight;
    register Float32 sample;
    register size_t dsti = ((size_t) dst) & 0x0000000F;

    assert(ctx->device->streamFormat.mChannelsPerFrame == 2);

    __alRecalcMonoSource(ctx, src);
    gainLeft = src->CalcGainLeft;
    gainRight = src->CalcGainRight;

    if (dsti)  // Destination isn't 16-byte aligned?
    {
        if ((dsti % 2) && (frames > dsti))  // bump dst to 16-byte alignment.
        {
            register Float32 convGainLeft = gainLeft * DIVBY32767;
            register Float32 convGainRight = gainRight * DIVBY32767;
            while (UNALIGNED_PTR(dst))
            {
                sample = ((Float32) *in);
                in++;
                dst[0] += sample * convGainLeft;
                dst[1] += sample * convGainRight;
                dst += 2;
                frames--;
            } // while
        } // if

        else  // dst will never align in this run; use scalar version.  :(
        {
            return(__alMixMono16(ctx, buf, src, dst, frames));
        } // else
    } // if


    overflow = frames - buflen;
    if (overflow < 0)
        overflow = 0;
    else
        frames = buflen;

    // Too quiet to be worth playing? Just skip ahead in the buffer appropriately.
    if ((gainLeft > MINVOL) || (gainRight > MINVOL))
    {
        // Permute vectors to convert mono channel to stereo.
        register vector unsigned char permute1 = (vector unsigned char)
                                                    ( 0x00, 0x01, 0x00, 0x01,
                                                      0x02, 0x03, 0x02, 0x03,
                                                      0x04, 0x05, 0x04, 0x05,
                                                      0x06, 0x07, 0x06, 0x07 );
        register vector unsigned char permute2;
        register vector unsigned char aligner = vec_lvsl(0, in);

        register vector float divby = (vector float) ( DIVBY32767, DIVBY32767,
                                                       DIVBY32767, DIVBY32767 );
        register vector float zero = (vector float) vec_splat_u32(0);
        register vector float gainvec;
        register vector signed int conv1;
        register vector signed int conv2;
        register vector signed int conv3;
        register vector signed int conv4;
        register vector float mixvec1;
        register vector float mixvec2;
        register vector float mixvec3;
        register vector float mixvec4;
        register vector float v1;
        register vector float v2;
        register vector float v3;
        register vector float v4;
        register vector signed short ld;
        register vector signed short ld2;
        register vector signed short ld_overflow;
        register int extra = (frames % 16);

        union
        {
            vector float v;
            float f[4];
        } gaincvt;

        permute2 = vec_add(permute1, vec_splat_u8(8));

        if (gainLeft > 1.0f)
            gainLeft = 1.0f;

        if (gainRight > 1.0f)
            gainRight = 1.0f;

        gaincvt.f[0] = gaincvt.f[2] = gainLeft;
        gaincvt.f[1] = gaincvt.f[3] = gainRight;
        gainvec = vec_madd(divby, gaincvt.v, zero);

        // Mix to the output buffer...
        frames -= extra;
        max = dst + (frames << 1);

        ld = vec_ld(0, in);  // read eight 16-bit samples.
        while (dst < max)
        {
            // Get the sample points into a vector register...
            ld_overflow = vec_ld(16, in);  // read eight 16-bit samples.

            mixvec1 = vec_ld(0x00, dst);
            mixvec2 = vec_ld(0x10, dst);
            mixvec3 = vec_ld(0x20, dst);
            mixvec4 = vec_ld(0x30, dst);

            in += 8;
            ld = vec_perm(ld, ld_overflow, aligner);

            // convert to stereo, so there's 4 stereo samples per vector.
            // ld  == 0l 0r 1l 1r 2l 2r 3l 3r
            // ld2 == 4l 4r 5l 5r 6l 6r 7l 6r
            ld2 = vec_perm(ld, ld, permute2);
            ld = vec_perm(ld, ld, permute1);

            // convert to 32-bit integers for vec_ctf...
            conv1 = vec_unpackh(ld);
            conv2 = vec_unpackl(ld);
            conv3 = vec_unpackh(ld2);
            conv4 = vec_unpackl(ld2);

            // convert from int to float...
            v1 = vec_ctf(conv1, 0);
            v2 = vec_ctf(conv2, 0);
            v3 = vec_ctf(conv3, 0);
            v4 = vec_ctf(conv4, 0);

            // change to float scale, apply gain, and mix with dst...
            v1 = vec_madd(v1, gainvec, mixvec1);
            v2 = vec_madd(v2, gainvec, mixvec2);
            v3 = vec_madd(v3, gainvec, mixvec3);
            v4 = vec_madd(v4, gainvec, mixvec4);

            // store converted version back to RAM...
            vec_st(v1, 0, dst);
            vec_st(v2, 16, dst);
            vec_st(v3, 32, dst);
            vec_st(v4, 48, dst);
            dst += 16;
            ld = ld_overflow;
        } // while

        // Handle overflow as scalar data.
        if (extra)
        {
            gainLeft *= DIVBY32767;
            gainRight *= DIVBY32767;
            max += extra;
            frames += extra;
            while (dst < max)
            {
                sample = ((Float32) *in);
                in++;
                dst[0] += sample * gainLeft;
                dst[1] += sample * gainRight;
                dst += 2;
            } // while
        } // if
    } // if

    src->bufferReadIndex += frames;
    return(overflow);
} // __alMixMono16_altivec


UInt32 __alMixStereo16_altivec(ALcontext *ctx, ALbuffer *buf, ALsource *src, Float32 *_dst, UInt32 frames)
{
    register UInt32 buflen = (buf->allocatedSpace >> 2) - src->bufferReadIndex;
    register ALsizei overflow = 0;
    register SInt16 *in = ((SInt16 *) buf->mixData) + (src->bufferReadIndex << 1);
    register Float32 *dst = _dst;
    register Float32 *max;
    register Float32 gain;

    assert(ctx->device->streamFormat.mChannelsPerFrame == 2);

    // !!! FIXME: Align dst, use vec_lvsl on in.
    if ( (UNALIGNED_PTR(dst)) || (UNALIGNED_PTR(in)) )
        return(__alMixStereo16(ctx, buf, src, dst, frames));

	gain = src->gain * ctx->listener.Gain;

    overflow = frames - buflen;
    if (overflow < 0)
        overflow = 0;
    else
        frames = buflen;

    // Too quiet to be worth playing? Just skip ahead in the buffer appropriately.
    if (gain > MINVOL)
    {
        register vector float divby = (vector float) ( DIVBY32767, DIVBY32767,
                                                       DIVBY32767, DIVBY32767 );
        register vector float zero = (vector float) vec_splat_u32(0);
        register vector float gainvec;
        register vector signed int conv1;
        register vector signed int conv2;
        register vector float mixvec1;
        register vector float mixvec2;
        register vector float v1;
        register vector float v2;
        register vector signed short ld;
        register int samples = (frames << 1);
        register int extra = (samples % 8);

        union
        {
            vector float v;
            float f[4];
        } gaincvt;

        if (gain > 1.0f)
            gain = 1.0f;

        gaincvt.f[0] = gaincvt.f[1] = gaincvt.f[2] = gaincvt.f[3] = gain;
        gainvec = vec_madd(divby, gaincvt.v, zero);

        // Mix to the output buffer...
        samples -= extra;
        max = dst + samples;

        while (dst < max)
        {
            // Get the sample points into a vector register...
            ld = vec_ld(0, in);
            in += 8;
            mixvec1 = vec_ld(0, dst);
            mixvec2 = vec_ld(16, dst);

            // convert to 32-bit integers for vec_ctf...
            conv1 = vec_unpackh(ld);
            conv2 = vec_unpackl(ld);

            // convert from int to float...
            v1 = vec_ctf(conv1, 0);
            v2 = vec_ctf(conv2, 0);

            // change to float scale, apply gain, and mix with dst...
            v1 = vec_madd(v1, gainvec, mixvec1);
            v2 = vec_madd(v2, gainvec, mixvec2);

            // store converted version back to RAM...
            vec_st(v1, 0, dst);
            vec_st(v2, 16, dst);
            dst += 8;
        } // while

        // Handle overflow as scalar data (2, 4, or 6 samples).
        if (extra)
        {
            gain *= DIVBY32767;
            max += extra;
            while (dst < max)
            {
                dst[0] += ((Float32) in[0]) * gain;
                dst[1] += ((Float32) in[1]) * gain;
                in += 2;
                dst += 2;
            } // while
        } // if
    } // if

    src->bufferReadIndex += frames;
    return(overflow);
} // __alMixStereo16_altivec

// end of alSoftware.c ...

