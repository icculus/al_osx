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

// This is crappy MP3 support from mpglib.
//  mpglib is also LGPL'd.
//  mpglib doesn't really tolerate data corruption.

#if !SUPPORTS_AL_EXT_MP3
#error You should not compile this without SUPPORTS_AL_EXT_MP3 defined.
#endif

#include "alInternal.h"
#include "mpg123_sdlsound.h"
#include "mpglib_sdlsound.h"

#if HAVE_PRAGMA_EXPORT
#pragma export off
#endif

typedef struct
{
    struct mpstr mp;
    UInt8 outbuf[8192];
    int outleft;
    int outpos;
} MP3Opaque;


static int doMp3Decode(MP3Opaque *o, void *outbuf, ALsizei outsize)
{
    int bw = 0;

    while (bw < outsize)
    {
        if (o->outleft > 0)
        {
            ALsizei cpysize = outsize - bw;
            if (cpysize > o->outleft)
                cpysize = o->outleft;
            memcpy(((UInt8 *) outbuf) + bw, o->outbuf + o->outpos, cpysize);
            bw += cpysize;
            o->outpos += cpysize;
            o->outleft -= cpysize;
            continue;
        } // if

        // need to decode more from the MP3 stream...
        o->outleft = o->outpos = 0;
        decodeMP3(&o->mp, NULL, 0, o->outbuf,
                  sizeof (o->outbuf), &o->outleft);
        if (o->outleft == 0)
            break;
    } // while

    return(bw / (sizeof (SInt16) * 2));
} // doMp3Decode


static UInt32 __alMixMP3(struct ALcontext_struct *ctx, struct ALbuffer_struct *buf,
                     struct ALsource_struct *src, Float32 *_dst, UInt32 frames)
{
    register UInt32 channels = ctx->device->streamFormat.mChannelsPerFrame;
    register MP3Opaque *opaque = (MP3Opaque *) src->opaque;
    register int samples = frames;
    int srcfreq = mpglib_freqs[opaque->mp.fr.sampling_frequency];
    int dstfreq = (int) ctx->device->streamFormat.mSampleRate;
    ALsizei outsize = samples * sizeof (SInt16) * 2;
    SInt16 *outbuf = (UInt16 *) alloca(outsize);
    int decoded = doMp3Decode(opaque, outbuf, outsize);

    src->bufferReadIndex = 1;  // hack to get this to not reinitialize the buffer in alContext.c.

    if (srcfreq != dstfreq)
    {
    	ALfloat ratio = ((ALfloat) dstfreq) / ((ALfloat) srcfreq);
        ALsizei newsize = ((ALsizei) ((outsize >> 2) * ratio)) << 2;
        SInt16 *resampled = (SInt16 *) alloca(newsize);
        __alResampleStereo16(outbuf, outsize, resampled, newsize);
        outbuf = resampled;
        outsize = newsize;
    } // if

    {  // !!! FIXME: make mixers generic against blocks of memory so I can
       // !!! FIXME:  use them here.
    register SInt16 *in = outbuf;
    register Float32 *dst;
    register Float32 *max;
    register Float32 gainLeft;
    register Float32 gainRight;

	gainLeft = gainRight = src->gain * ctx->listener.Gain;
    gainLeft *= ctx->device->speakergains[0];
    gainRight *= ctx->device->speakergains[1];

    // Too quiet to be worth playing? Just skip ahead in the buffer appropriately.
    if ((gainLeft > MINVOL) || (gainRight > MINVOL))
    {
        // Mix to the output buffer...
        dst = _dst;
        max = dst + (decoded * channels);
        in = (SInt16 *) outbuf;

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
    }

    return(frames - decoded);
} // __alMixMP3


static ALboolean __alPrepareBufferMP3(ALcontext *ctx,
                                      ALsource *src,
                                      ALbuffer *buf)
{
    MP3Opaque *o = calloc(1, sizeof (MP3Opaque));
    if (o == NULL)
    {
        __alSetError(AL_OUT_OF_MEMORY);
        return(AL_FALSE);
    } // if

    InitMP3(&o->mp);
    decodeMP3(&o->mp, buf->mixData, buf->allocatedSpace,
              (char *) &o->outbuf, sizeof (o->outbuf), &o->outleft);

    src->opaque = o;
    return(AL_TRUE);
} // __alPrepareBufferMP3


static ALvoid __alProcessedBufferMP3(ALsource *src, ALbuffer *buf)
{
    MP3Opaque *opaque = (MP3Opaque *) src->opaque;
    if (opaque != NULL)
    {
        ExitMP3(&opaque->mp);
        free(opaque);
    } // if
} // __alProcessedBufferMP3


static ALvoid __alRewindBufferMP3(ALsource *src, ALbuffer *buf)
{
    MP3Opaque *o = (MP3Opaque *) src->opaque;
    if (o != NULL)
    {
        ExitMP3(&o->mp);
        InitMP3(&o->mp);
        decodeMP3(&o->mp, buf->mixData, buf->allocatedSpace,
                  (char *) &o->outbuf, sizeof (o->outbuf), &o->outleft);
        o->outpos = 0;
    } // if
} // __alRewindBufferMP3


static ALvoid __alDeleteBufferMP3(ALbuffer *buf)
{
    // no-op ... we don't allocate anything as buffer-instance data currently.
} // __alDeleteBufferMP3


ALvoid __alBufferDataFromMP3(ALbuffer *buf)
{
    buf->prepareBuffer = __alPrepareBufferMP3;
    buf->processedBuffer = __alProcessedBufferMP3;
    buf->rewindBuffer = __alRewindBufferMP3;
    buf->deleteBuffer = __alDeleteBufferMP3;
    buf->mixFunc = __alMixMP3;
} // __alBufferDataFromMP3

// end of al_ext_mp3.c ...

