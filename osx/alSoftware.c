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

ALboolean __alRingBufferInit(__ALRingBuffer *ring, ALsizei size)
{
    UInt8 *ptr = (UInt8 *) realloc(ring->buffer, size);
    if (ptr == NULL)
        return(AL_FALSE);

    ring->buffer = ptr;
    ring->size = size;
    ring->write = 0;
    ring->read = 0;
    ring->used = 0;
    return(AL_TRUE);
} // __alRingBufferInit

ALvoid __alRingBufferShutdown(__ALRingBuffer *ring)
{
    free(ring->buffer);
    ring->buffer = NULL;
} // __alRingBufferShutdown

ALsizei __alRingBufferSize(__ALRingBuffer *ring)
{
    return(ring->used);
} // __alRingBufferSize

ALvoid __alRingBufferPut(__ALRingBuffer *ring, UInt8 *data, ALsizei _size)
{
    register ALsizei size = _size;
    register ALsizei cpy;
    register ALsizei avail;

    if (!size)   // just in case...
        return;

    // Putting more data than ring buffer holds in total? Replace it all.
    if (size > ring->size)
    {
        ring->write = 0;
        ring->read = 0;
        ring->used = ring->size;
        memcpy(ring->buffer, data + (size - ring->size), ring->size);
        return;
    } // if

    // Buffer overflow? Push read pointer to oldest sample not overwritten...
    avail = ring->size - ring->used;
    if (size > avail)
    {
        ring->read += size - avail;
        if (ring->read > ring->size)
            ring->read -= ring->size;
    } // if

    // Clip to end of buffer and copy first block...
    cpy = ring->size - ring->write;
    if (size < cpy)
        cpy = size;
    if (cpy) memcpy(ring->buffer + ring->write, data, cpy);

    // Wrap around to front of ring buffer and copy remaining data...
    avail = size - cpy;
    if (avail) memcpy(ring->buffer, data + cpy, avail);

    // Update write pointer...
    ring->write += size;
    if (ring->write > ring->size)
        ring->write -= ring->size;

    ring->used += size;
    if (ring->used > ring->size)
        ring->used = ring->size;
} // __alRingBufferPut


ALsizei __alRingBufferGet(__ALRingBuffer *ring, UInt8 *data, ALsizei _size)
{
    register ALsizei cpy;
    register ALsizei size = _size;
    register ALsizei avail = ring->used;

    // Clamp amount to read to available data...
    if (size > avail)
        size = avail;
    
    // Clip to end of buffer and copy first block...
    cpy = ring->size - ring->read;
    if (cpy > size) cpy = size;
    if (cpy) memcpy(data, ring->buffer + ring->read, cpy);
    
    // Wrap around to front of ring buffer and copy remaining data...
    avail = size - cpy;
    if (avail) memcpy(data + cpy, ring->buffer, avail);

    // Update read pointer...
    ring->read += size;
    if (ring->read > ring->size)
        ring->read -= ring->size;

    ring->used -= size;

    return(size);  // may have been clamped if there wasn't enough data...
} // __alRingBufferGet


// Converters ...
ALboolean __alConvertFromMonoFloat32(Float32 *s, UInt8 *d,
                                     ALenum fmt, ALsizei samples)
{
    // !!! FIXME: Altivec this!
    register Float32 *src = s;
    register ALsizei i;

    switch (fmt)
    {
        #if SUPPORTS_AL_EXT_FLOAT32
        case AL_FORMAT_MONO_FLOAT32:
            memcpy(d, src, samples * sizeof (Float32));
            return(AL_TRUE);

        case AL_FORMAT_STEREO_FLOAT32:
        {
            register Float32 samp;
            register Float32 *dst = (Float32 *) d;
            for (i = 0; i < samples; i++)
            {
                samp = *src;
                dst[0] = samp;
                dst[1] = samp;
                dst += 2;
                src++;
            } // for
            return(AL_TRUE);
        } // case
        #endif

        case AL_FORMAT_MONO8:
        {
            register UInt8 *dst = (UInt8 *) d;
            for (i = 0; i < samples; i++)
            {
                *dst = (UInt8) ((*src * 127.0f) + 127.0f);
                dst++;
                src++;
            } // for
            return(AL_TRUE);
        } // case

        case AL_FORMAT_STEREO8:
        {
            register UInt8 *dst = (UInt8 *) d;
            register UInt8 val;
            for (i = 0; i < samples; i++)
            {
                val = (UInt8) ((*src * 127.0f) + 127.0f);
                dst[0] = val;
                dst[1] = val;
                dst += 2;
                src++;
            } // for
            return(AL_TRUE);
        } // case

        case AL_FORMAT_MONO16:
        {
            register SInt16 *dst = (SInt16 *) d;
            for (i = 0; i < samples; i++)
            {
                *dst = (SInt16) (*src * 32767.0f);
                dst++;
                src++;
            } // for
            return(AL_TRUE);
        } // case

        case AL_FORMAT_STEREO16:
        {
            register SInt16 *dst = (SInt16 *) d;
            register SInt16 val;
            for (i = 0; i < samples; i++)
            {
                val = (SInt16) (*src * 32767.0f);
                dst[0] = val;
                dst[1] = val;
                dst += 2;
                src++;
            } // for
            return(AL_TRUE);
        } // case
    } // switch

    return(AL_FALSE);
} // __alConvertFromMonoFloat32


ALboolean __alConvertFromStereoFloat32(Float32 *s, UInt8 *d,
                                       ALenum fmt, ALsizei samples)
{
    // !!! FIXME: Altivec this!
    register Float32 samp;
    register Float32 *src = s;
    register ALsizei i;

    switch (fmt)
    {
        #if SUPPORTS_AL_EXT_FLOAT32
        case AL_FORMAT_MONO_FLOAT32:
        {
            register Float32 *dst = (Float32 *) d;
            for (i = 0; i < samples; i++)
            {
                *dst = (src[0] + src[1]) * 0.5f;
                dst++;
                src += 2;
            } // for
            return(AL_TRUE);
        } // case

        case AL_FORMAT_STEREO_FLOAT32:
            memcpy(d, src, samples * (sizeof (Float32) * 2));
            return(AL_TRUE);
        #endif

        case AL_FORMAT_MONO8:
        {
            register UInt8 *dst = (UInt8 *) d;
            for (i = 0; i < samples; i++)
            {
                samp = ((src[0] + src[1]) * 0.5f);
                *dst = (UInt8) ((samp * 127.0f) + 127.0f);
                dst++;
                src += 2;
            } // for
            return(AL_TRUE);
        } // case

        case AL_FORMAT_STEREO8:
        {
            register UInt8 *dst = (UInt8 *) d;
            samples <<= 1;
            for (i = 0; i < samples; i++)
            {
                *dst = (UInt8) ((*src * 127.0f) + 127.0f);
                dst++;
                src++;
            } // for
            return(AL_TRUE);
        } // case

        case AL_FORMAT_MONO16:
        {
            register SInt16 *dst = (SInt16 *) d;
            for (i = 0; i < samples; i++)
            {
                samp = ((src[0] + src[1]) * 0.5f);
                *dst = (SInt16) (samp * 32767.0f);
                dst++;
                src += 2;
            } // for
            return(AL_TRUE);
        } // case

        case AL_FORMAT_STEREO16:
        {
            register SInt16 *dst = (SInt16 *) d;
            samples <<= 1;
            for (i = 0; i < samples; i++)
            {
                *dst = (SInt16) (*src * 32767.0f);
                dst++;
                src++;
            } // for
            return(AL_TRUE);
        } // case
    } // switch

    return(AL_FALSE);
} // __alConvertFromStereoFloat32



ALvoid __alRecalcMonoSource(ALcontext *ctx, ALsource *src)
{
#if DO_DOPPLER
    ALvector SourceListenerDir;
#endif
    ALvector Position;
    ALvector Velocity;
	ALvector U;
	ALvector V;
	ALvector N;
	ALfloat Matrix[3][3];
    ALfloat Distance;
	ALfloat minGain, maxGain;
    ALfloat CalcVolume;
    ALenum distanceModel;
    ALlistener *listener;

    // Only process if some source attribute has changed.
//    if (!src->needsRecalculation)
//        return;

    src->needsRecalculation = AL_FALSE;

    distanceModel = ctx->distanceModel;
    listener = &ctx->listener;

	// Convert source position to listener's coordinate system
    minGain = src->minGain;
    maxGain = src->maxGain;
    __alVectorAssign3(Position, src->Position);
    __alVectorAssign3(Velocity, src->Velocity);

	// Translate Listener to origin if not in AL_SOURCE_RELATIVE mode
	if (src->srcRelative == AL_FALSE)
        __alVectorSubtract3(Position, listener->Position);

#if DO_DOPPLER
    // Set SourceListenerDir to be used later for doppler
    __alVectorAssign3(SourceListenerDir, Position);
#endif

    // Align coordinate system axis
    __alCrossproduct(listener->Up,listener->Forward,U);
    __alNormalize(U);
    __alCrossproduct(listener->Forward,U,V);
    __alNormalize(V);
    __alVectorAssign3(N, listener->Forward);
    __alNormalize(N);
    Matrix[0][0]=U[0]; Matrix[0][1]=V[0]; Matrix[0][2]=-N[0];
    Matrix[1][0]=U[1]; Matrix[1][1]=V[1]; Matrix[1][2]=-N[1];
    Matrix[2][0]=U[2]; Matrix[2][1]=V[2]; Matrix[2][2]=-N[2];
    __alMatrixVector(Matrix,Position);
    // Convert into falloff and panning
    Distance = __alSqrt(Position[0]*Position[0]+Position[1]*Position[1]+Position[2]*Position[2]);

    if ((distanceModel != AL_NONE) && (Distance > 0.0f))
    {
        if (distanceModel == AL_INVERSE_DISTANCE_CLAMPED)
        {
            if (Distance < src->referenceDistance)
                Distance = src->referenceDistance;
            if (Distance > src->maxDistance)
                Distance = src->maxDistance;
        } // if
        CalcVolume=(src->gain*listener->Gain*src->referenceDistance)/(src->referenceDistance+src->rolloffFactor*(Distance-src->referenceDistance));
    } // if
    else
    {
        CalcVolume=(src->gain*listener->Gain);
    } // else

    // cap Volume by min/max gains
    if (CalcVolume < minGain)
        CalcVolume = minGain;
    else if (CalcVolume > maxGain)
        CalcVolume = maxGain;

    if (CalcVolume > 1.0f)
        CalcVolume = 1.0f;

    // !!! FIXME: Where the hell did this come from?
    //CalcVolume *= FULL_VOLUME;

    if (ctx->device->speakerConfig == SPKCFG_STDSTEREO)
    {
        if (!EPSILON_EQUAL(Distance, 0.0f))
        {
            register ALfloat Panning;

            // this is broken, pushes everything to the right.
            //Panning = 2.0f * ( (0.5f + (0.5f * 0.707f)) *
            //                   cosf(atan2f(Position[2],Position[0])) );

            // Jonas Echterhoff caught the problem...too many parentheses!
            Panning = 2.0f * ( 0.5f + (0.5f * 0.707f) *
                                (ALfloat) (cos(atan2f(Position[2],Position[0]))) );

    	    src->channelGain0 = CalcVolume * Panning * ctx->device->speakergains[0];
            src->channelGain1 = CalcVolume * (2.0f - Panning) * ctx->device->speakergains[1];
        } // if
        else
        {
            src->channelGain0 = CalcVolume * ctx->device->speakergains[0];
            src->channelGain1 = CalcVolume * ctx->device->speakergains[1];
        } // else

        CLAMP(src->channelGain0, 0.0f, 1.0f);
        CLAMP(src->channelGain1, 0.0f, 1.0f);
    } // if

    else if (ctx->device->speakerConfig == SPKCFG_POSATTENUATION)
    {
        assert(0); // !!! FIXME
    } // else if

    else
    {
        #if USE_VBAP
        __alDoVBAP(ctx, src, Position, CalcVolume);
        #else
        assert(0);  // houston, we have a problem. Not stereo, but no VBAP?!
        #endif
    } // else

#if DO_DOPPLER
    // Calculate doppler
    if (!((SourceListenerDir[0]==0)&&(SourceListenerDir[1]==0)&&(SourceListenerDir[2]==0)))
    {
        ALfloat CalcPitch = src->pitch;
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
        src->pitch = CalcPitch;
    } // if
#endif
} // __alRecalcMonoSource


// Mixing...

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
    register Float32 sample;
    register Float32 gain0;
    register Float32 gain1;
    register Float32 gain2;
    register ALsizei channel0;
    register ALsizei channel1;
    register ALsizei channel2;

    __alRecalcMonoSource(ctx, src);

	gain0 = src->channelGain0 * DIVBY127;
	gain1 = src->channelGain1 * DIVBY127;
    gain2 = src->channelGain2 * DIVBY127;
	channel0 = src->channel0;
	channel1 = src->channel1;
	channel2 = src->channel2;

    overflow = frames - buflen;
    if (overflow < 0)
        overflow = 0;
    else
        frames = buflen;

    // Too quiet to be worth playing? Just skip ahead in the buffer appropriately.
    if ((gain0 > MINVOL) || (gain1 > MINVOL) || (gain2 > MINVOL))
    {
        // Mix to the output buffer...
        channels = ctx->device->streamFormat.mChannelsPerFrame;
        dst = _dst;
        max = dst + (frames * channels);
        in = ((SInt8 *) buf->mixData) + src->bufferReadIndex;

        while (dst < max)
        {
            sample = ((Float32) *in);
            in++;
            dst[channel0] += sample * gain0;
            dst[channel1] += sample * gain1;
            dst[channel2] += sample * gain2;
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
    register Float32 sample;
    register Float32 gain0;
    register Float32 gain1;
    register Float32 gain2;
    register ALsizei channel0;
    register ALsizei channel1;
    register ALsizei channel2;

    __alRecalcMonoSource(ctx, src);

	gain0 = src->channelGain0;
	gain1 = src->channelGain1;
    gain2 = src->channelGain2;
	channel0 = src->channel0;
	channel1 = src->channel1;
	channel2 = src->channel2;

    overflow = frames - buflen;
    if (overflow < 0)
        overflow = 0;
    else
        frames = buflen;

    // Too quiet to be worth playing? Just skip ahead in the buffer appropriately.

    if ((gain0 > MINVOL) || (gain1 > MINVOL) || (gain2 > MINVOL))
    {
        // Mix to the output buffer...
    	gain0 *= DIVBY32767;
	    gain1 *= DIVBY32767;
        gain2 *= DIVBY32767;
        channels = ctx->device->streamFormat.mChannelsPerFrame;
        dst = _dst;
        max = dst + (frames * channels);
        in = ((SInt16 *) buf->mixData) + src->bufferReadIndex;
        while (dst < max)
        {
            sample = ((Float32) *in);
            in++;
            dst[channel0] += sample * gain0;
            dst[channel1] += sample * gain1;
            dst[channel2] += sample * gain2;
            dst += channels;  // skip rest of channels.
        } // while
    } // if

    src->bufferReadIndex += frames;
    return(overflow);
} // __alMixMono16


// !!! FIXME: Stereo samples don't spatialize; what speakers should be used
// !!! FIXME:  in a > stereo configurations?
UInt32 __alMixStereo8(ALcontext *ctx, ALbuffer *buf, ALsource *src, Float32 *_dst, UInt32 frames)
{
    register UInt32 buflen = (buf->allocatedSpace >> 1) - src->bufferReadIndex;
    register UInt32 channels;
    register ALsizei overflow = 0;
    register SInt8 *in;
    register Float32 *dst;
    register Float32 *max;
    register Float32 gainLeft;
    register Float32 gainRight;

	gainLeft = gainRight = src->gain * ctx->listener.Gain;
    gainLeft *= ctx->device->speakergains[0];
    gainRight *= ctx->device->speakergains[1];

    overflow = frames - buflen;
    if (overflow < 0)
        overflow = 0;
    else
        frames = buflen;

    // Too quiet to be worth playing? Just skip ahead in the buffer appropriately.
    if ((gainLeft > MINVOL) || (gainRight > MINVOL))
    {
        // Mix to the output buffer...
        channels = ctx->device->streamFormat.mChannelsPerFrame;
        dst = _dst;
        max = dst + (frames * channels);
        in = ((SInt8 *) buf->mixData) + (src->bufferReadIndex << 1);

        gainLeft *= DIVBY127;
        gainRight *= DIVBY127;
        while (dst < max)
        {
            dst[0] += ((Float32) in[0]) * gainLeft;
            dst[1] += ((Float32) in[1]) * gainRight;
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
    register Float32 gainLeft;
    register Float32 gainRight;

	gainLeft = gainRight = src->gain * ctx->listener.Gain;
    gainLeft *= ctx->device->speakergains[0];
    gainRight *= ctx->device->speakergains[1];

    overflow = frames - buflen;
    if (overflow < 0)
        overflow = 0;
    else
        frames = buflen;

    // Too quiet to be worth playing? Just skip ahead in the buffer appropriately.
    if ((gainLeft > MINVOL) || (gainRight > MINVOL))
    {
        // Mix to the output buffer...
        channels = ctx->device->streamFormat.mChannelsPerFrame;
        dst = _dst;
        max = dst + (frames * channels);
        in = ((SInt16 *) buf->mixData) + (src->bufferReadIndex << 1);

        gainLeft *= DIVBY32767;
        gainRight *= DIVBY32767;
        while (dst < max)
        {
            dst[0] += ((Float32) in[0]) * gainLeft;
            dst[1] += ((Float32) in[1]) * gainRight;
            in += 2;
            dst += channels;  // skip rest of channels.
        } // while
    } // if

    src->bufferReadIndex += frames;
    return(overflow);
} // __alMixStereo16


// !!! FIXME: Altivec mixers only work in stereo configurations.
UInt32 __alMixMono8_altivec(ALcontext *ctx, ALbuffer *buf, ALsource *src, Float32 *_dst, UInt32 frames)
{
    register ALsizei overflow = 0;

    #if __POWERPC__
    register UInt32 buflen = buf->allocatedSpace - src->bufferReadIndex;
    register SInt8 *in = ((SInt8 *) buf->mixData) + src->bufferReadIndex;
    register Float32 *dst = _dst;
    register Float32 *max;
    register Float32 gainLeft;
    register Float32 gainRight;
    register Float32 sample;
    register size_t dsti = ((size_t) dst) & 0x0000000F;

    assert(ctx->device->streamFormat.mChannelsPerFrame == 2);

    __alRecalcMonoSource(ctx, src);
	gainLeft = src->channelGain0;
	gainRight = src->channelGain1;

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
    #endif // __POWERPC__

    return(overflow);
} // __alMixMono8_altivec


UInt32 __alMixMono16_altivec(ALcontext *ctx, ALbuffer *buf, ALsource *src, Float32 *_dst, UInt32 frames)
{
    register ALsizei overflow = 0;

    #if __POWERPC__
    //register UInt32 ctrl_in = DST_BLOCK_CTRL(1, 2, 16);
    //register UInt32 ctrl_out = DST_BLOCK_CTRL(1, 8, 16);
    register UInt32 buflen = (buf->allocatedSpace >> 1) - src->bufferReadIndex;
    register SInt16 *in = ((SInt16 *) buf->mixData) + src->bufferReadIndex;
    register Float32 *dst = _dst;
    register Float32 *max;
    register Float32 gainLeft;
    register Float32 gainRight;
    register Float32 sample;
    register size_t dsti = ((size_t) dst) & 0x0000000F;

    assert(ctx->device->streamFormat.mChannelsPerFrame == 2);

    __alRecalcMonoSource(ctx, src);
	gainLeft = src->channelGain0;
	gainRight = src->channelGain1;

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

        gaincvt.f[0] = gaincvt.f[2] = gainLeft;
        gaincvt.f[1] = gaincvt.f[3] = gainRight;
        gainvec = vec_madd(divby, gaincvt.v, zero);

        // Mix to the output buffer...
        frames -= extra;
        max = dst + (frames << 1);

        ld = vec_ld(0, in);  // read eight 16-bit samples.
        while (dst < max)
        {
            //vec_dstt(in, ctrl_in, 0);
            //vec_dstst(dst, ctrl_out, 1);

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
            vec_st(v1, 0x00, dst);
            vec_st(v2, 0x10, dst);
            vec_st(v3, 0x20, dst);
            vec_st(v4, 0x30, dst);
            dst += 16;
            ld = ld_overflow;
        } // while

        //vec_dss(0);
        //vec_dss(1);

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
    #endif // __POWERPC__

    return(overflow);
} // __alMixMono16_altivec


UInt32 __alMixStereo16_altivec(ALcontext *ctx, ALbuffer *buf, ALsource *src, Float32 *_dst, UInt32 frames)
{
    register ALsizei overflow = 0;

    #if __POWERPC__
    register UInt32 buflen = (buf->allocatedSpace >> 2) - src->bufferReadIndex;
    register SInt16 *in = ((SInt16 *) buf->mixData) + (src->bufferReadIndex << 1);
    register Float32 *dst = _dst;
    register Float32 *max;
    register Float32 gainLeft;
    register Float32 gainRight;

    assert(ctx->device->streamFormat.mChannelsPerFrame == 2);

    // !!! FIXME: Align dst, use vec_lvsl on in.
    if ( (UNALIGNED_PTR(dst)) || (UNALIGNED_PTR(in)) )
        return(__alMixStereo16(ctx, buf, src, dst, frames));

	gainLeft = gainRight = src->gain * ctx->listener.Gain;
    gainLeft *= ctx->device->speakergains[0];
    gainRight *= ctx->device->speakergains[1];

    overflow = frames - buflen;
    if (overflow < 0)
        overflow = 0;
    else
        frames = buflen;

    // Too quiet to be worth playing? Just skip ahead in the buffer appropriately.
    if ((gainLeft > MINVOL) || (gainRight > MINVOL))
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

        gaincvt.f[0] = gaincvt.f[2] = gainLeft;
        gaincvt.f[1] = gaincvt.f[3] = gainRight;
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
            gainLeft *= DIVBY32767;
            gainRight *= DIVBY32767;
            max += extra;
            while (dst < max)
            {
                dst[0] += ((Float32) in[0]) * gainLeft;
                dst[1] += ((Float32) in[1]) * gainRight;
                in += 2;
                dst += 2;
            } // while
        } // if
    } // if

    src->bufferReadIndex += frames;
    #endif // __POWERPC__

    return(overflow);
} // __alMixStereo16_altivec


// !!! FIXME: Float32 codepath is totally untested.
#if SUPPORTS_AL_EXT_FLOAT32
UInt32 __alMixMonoFloat32(ALcontext *ctx, ALbuffer *buf, ALsource *src, Float32 *_dst, UInt32 frames)
{
    register UInt32 buflen = (buf->allocatedSpace >> 2) - src->bufferReadIndex;
    register UInt32 channels;
    register ALsizei overflow = 0;
    register Float32 *in;
    register Float32 *dst;
    register Float32 *max;
    register Float32 sample;
    register Float32 gain0;
    register Float32 gain1;
    register Float32 gain2;
    register ALsizei channel0;
    register ALsizei channel1;
    register ALsizei channel2;

    __alRecalcMonoSource(ctx, src);

	gain0 = src->channelGain0 * DIVBY32767;
	gain1 = src->channelGain1 * DIVBY32767;
    gain2 = src->channelGain2 * DIVBY32767;
	channel0 = src->channel0;
	channel1 = src->channel1;
	channel2 = src->channel2;

    overflow = frames - buflen;
    if (overflow < 0)
        overflow = 0;
    else
        frames = buflen;

    // Too quiet to be worth playing? Just skip ahead in the buffer appropriately.
    if ((gain0 > MINVOL) || (gain1 > MINVOL) || (gain2 > MINVOL))
    {
        // Mix to the output buffer...
        channels = ctx->device->streamFormat.mChannelsPerFrame;
        dst = _dst;
        max = dst + (frames * channels);
        in = ((Float32 *) buf->mixData) + src->bufferReadIndex;
        while (dst < max)
        {
            sample = ((Float32) *in);
            in++;
            dst[channel0] += sample * gain0;
            dst[channel1] += sample * gain1;
            dst[channel2] += sample * gain2;
            dst += channels;  // skip rest of channels.
        } // while
    } // if

    src->bufferReadIndex += frames;
    return(overflow);
} // __alMixMonoFloat32


UInt32 __alMixStereoFloat32(ALcontext *ctx, ALbuffer *buf, ALsource *src, Float32 *_dst, UInt32 frames)
{
    register UInt32 buflen = (buf->allocatedSpace >> 3) - src->bufferReadIndex;
    register UInt32 channels;
    register ALsizei overflow = 0;
    register Float32 *in;
    register Float32 *dst;
    register Float32 *max;
    register Float32 gainLeft;
    register Float32 gainRight;

	gainLeft = gainRight = src->gain * ctx->listener.Gain;
    gainLeft *= ctx->device->speakergains[0];
    gainRight *= ctx->device->speakergains[1];

    overflow = frames - buflen;
    if (overflow < 0)
        overflow = 0;
    else
        frames = buflen;

    // Too quiet to be worth playing? Just skip ahead in the buffer appropriately.
    if ((gainLeft > MINVOL) || (gainRight > MINVOL))
    {
        // Mix to the output buffer...
        channels = ctx->device->streamFormat.mChannelsPerFrame;
        dst = _dst;
        max = dst + (frames * channels);
        in = ((Float32 *) buf->mixData) + (src->bufferReadIndex << 1);

        while (dst < max)
        {
            dst[0] += in[0] * gainLeft;
            dst[1] += in[1] * gainRight;
            in += 2;
            dst += channels;  // skip rest of channels.
        } // while
    } // if

    src->bufferReadIndex += frames;
    return(overflow);
} // __alMixStereoFloat32


UInt32 __alMixMonoFloat32_altivec(ALcontext *ctx, ALbuffer *buf, ALsource *src, Float32 *_dst, UInt32 frames)
{
    register ALsizei overflow = 0;

    #if __POWERPC__
    register UInt32 buflen = (buf->allocatedSpace >> 2) - src->bufferReadIndex;
    register Float32 *in = ((Float32 *) buf->mixData) + src->bufferReadIndex;
    register Float32 *dst = _dst;
    register Float32 *max;
    register Float32 gainLeft;
    register Float32 gainRight;
    register Float32 sample;
    register size_t dsti = ((size_t) dst) & 0x0000000F;

    assert(ctx->device->streamFormat.mChannelsPerFrame == 2);

    __alRecalcMonoSource(ctx, src);
	gainLeft = src->channelGain0;
	gainRight = src->channelGain1;

    if (dsti)  // Destination isn't 16-byte aligned?
    {
        if ((dsti % 8) && (frames > dsti))  // bump dst to 16-byte alignment.
        {
            // !!! FIXME: Clamp gain?
            while (UNALIGNED_PTR(dst))
            {
                sample = *in;
                in++;
                dst[0] += sample * gainLeft;
                dst[1] += sample * gainRight;
                dst += 2;
                frames--;
            } // while
        } // if

        else  // dst will never align in this run; use scalar version.  :(
        {
            return(__alMixMonoFloat32(ctx, buf, src, dst, frames));
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
        register vector unsigned char permute1 = VPERMUTE4(0, 0, 1, 1);
        register vector unsigned char permute2;
        register vector unsigned char aligner = vec_lvsl(0, in);
        register vector float gainvec;
        register vector float mixvec1;
        register vector float mixvec2;
        register vector float ld;
        register vector float ld2;
        register vector float ld_overflow;
        register int extra = (frames % 16);

        union
        {
            vector float v;
            float f[4];
        } gaincvt;

        gaincvt.f[0] = gaincvt.f[2] = gainLeft;
        gaincvt.f[1] = gaincvt.f[3] = gainRight;

        permute2 = vec_add(permute1, vec_splat_u8(8));
        gainvec = vec_ld(0x00, &gaincvt.v);

        // Mix to the output buffer...
        frames -= extra;
        max = dst + (frames << 1);

        ld = vec_ld(0, in);  // read four 32-bit samples.
        while (dst < max)
        {
            // Get the sample points into a vector register...
            ld_overflow = vec_ld(16, in);  // read four 32-bit samples.

            mixvec1 = vec_ld(0x00, dst);
            mixvec2 = vec_ld(0x10, dst);

            in += 4;
            ld = vec_perm(ld, ld_overflow, aligner);

            // convert to stereo, so there's 4 stereo samples per vector.
            // ld  == 0l 0r 1l 1r 2l 2r 3l 3r
            // ld2 == 4l 4r 5l 5r 6l 6r 7l 6r
            ld2 = vec_perm(ld, ld, permute2);
            ld = vec_perm(ld, ld, permute1);

            // Apply gain and mix with dst...
            ld2 = vec_madd(ld, gainvec, mixvec2);
            ld = vec_madd(ld, gainvec, mixvec1);

            // store converted version back to RAM...
            vec_st(ld, 0x00, dst);
            vec_st(ld2, 0x10, dst);
            dst += 8;
            ld = ld_overflow;
        } // while

        // Handle overflow as scalar data.
        if (extra)
        {
            max += extra;
            frames += extra;
            while (dst < max)
            {
                sample = *in;
                in++;
                dst[0] += sample * gainLeft;
                dst[1] += sample * gainRight;
                dst += 2;
            } // while
        } // if
    } // if

    src->bufferReadIndex += frames;
    #endif  // __POWERPC__

    return(overflow);
} // __alMixMonoFloat32_altivec


UInt32 __alMixStereoFloat32_altivec(ALcontext *ctx, ALbuffer *buf, ALsource *src, Float32 *_dst, UInt32 frames)
{
    register ALsizei overflow = 0;

    #if __POWERPC__
    register UInt32 buflen = (buf->allocatedSpace >> 3) - src->bufferReadIndex;
    register Float32 *in = ((Float32 *) buf->mixData) + (src->bufferReadIndex << 1);
    register Float32 *dst = _dst;
    register Float32 *max;
    register Float32 gainLeft;
    register Float32 gainRight;

    assert(ctx->device->streamFormat.mChannelsPerFrame == 2);

    // !!! FIXME: Align dst, use vec_lvsl on in.
    if ( (UNALIGNED_PTR(dst)) || (UNALIGNED_PTR(in)) )
        return(__alMixStereoFloat32(ctx, buf, src, dst, frames));

	gainLeft = gainRight = src->gain * ctx->listener.Gain;
    gainLeft *= ctx->device->speakergains[0];
    gainRight *= ctx->device->speakergains[1];

    overflow = frames - buflen;
    if (overflow < 0)
        overflow = 0;
    else
        frames = buflen;

    // Too quiet to be worth playing? Just skip ahead in the buffer appropriately.
    if ((gainLeft > MINVOL) || (gainRight > MINVOL))
    {
        register vector float gainvec;
        register vector float mixvec1;
        register vector float ld;
        register int samples = (frames << 1);
        register int extra = (samples % 8);

        union
        {
            vector float v;
            float f[4];
        } gaincvt;

        gaincvt.f[0] = gaincvt.f[2] = gainLeft;
        gaincvt.f[1] = gaincvt.f[3] = gainRight;
        gainvec = vec_ld(0x00, &gaincvt.v);

        // Mix to the output buffer...
        samples -= extra;
        max = dst + samples;

        while (dst < max)
        {
            // Get the sample points into a vector register...
            ld = vec_ld(0, in);
            in += 4;
            mixvec1 = vec_ld(0, dst);

            // Apply gain and mix with dst...
            mixvec1 = vec_madd(ld, gainvec, mixvec1);

            // store converted version back to RAM...
            vec_st(mixvec1, 0x00, dst);
            dst += 4;
        } // while

        // Handle overflow as scalar data
        if (extra)
        {
            max += extra;
            while (dst < max)
            {
                dst[0] += in[0] * gainLeft;
                dst[1] += in[1] * gainRight;
                in += 2;
                dst += 2;
            } // while
        } // if
    } // if

    src->bufferReadIndex += frames;
    #endif  // __POWERPC__

    return(overflow);
} // __alMixStereoFloat32_altivec
#endif

// end of alSoftware.c ...

