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


// The buffer name we hand back to the application at generation time
//  is the index into the context "buffers" table, plus one. This is
//  because zero is defined as a null buffer, so you can disassociate
//  a source from buffers altogether by telling it to use buffer zero.
// To avoid giving the app a valid buffer name of zero, we add one to the
//  name, and adjust for this when referencing it (refer to
//  __alGrabContextAndGetBuffer(), alGenBuffers() and alDeleteBuffers().)


#if HAVE_PRAGMA_EXPORT
#pragma export off
#endif

ALvoid __alBuffersInit(ALbuffer *bufs, ALsizei count)
{
    memset(bufs, '\0', sizeof (ALbuffer) * count);
} // __alBuffersInit


ALvoid __alBuffersShutdown(ALbuffer *bufs, ALsizei count)
{
    int i;
    int max = count;
    ALbuffer *buf = bufs;

    for (i = 0; i < max; i++, buf++)
    {
        free(buf->mixUnalignedPtr);
        if (buf->deleteBuffer != NULL)
            buf->deleteBuffer(buf);
    } // for

    memset(bufs, '\0', sizeof (ALbuffer) * count);
} // __alBuffersShutdown


static ALcontext *__alGrabContextAndGetBuffer(ALuint bufid, ALbuffer **bufout)
{
    register ALcontext *ctx;

    bufid--;  // account for Buffer Zero.

	if (bufid > AL_MAXBUFFERS)
    {
	    __alSetError(AL_INVALID_NAME);
        return(NULL);
    } // if

    ctx = __alGrabCurrentContext();
    *bufout = &ctx->buffers[bufid];

    if ( !((*bufout)->inUse) )
    {
	    __alSetError(AL_INVALID_NAME);
        __alUngrabContext(ctx);
        return(NULL);
    } // if

    return(ctx);
} // __alGrabContextAndGetBuffer



#if HAVE_PRAGMA_EXPORT
#pragma export on
#endif

ALAPI ALvoid ALAPIENTRY alGenBuffers(ALsizei n, ALuint *buffers)
{
	register int i;
	register int iCount = 0;
    register ALbuffer *buf;
    register ALcontext *ctx;

	if (n == 0)
		return; // legal no-op.

	else if (n < 0)
    {
		__alSetError(AL_INVALID_VALUE);
		return;
	} // else if

    ctx = __alGrabCurrentContext();
    if (ctx == NULL)
        return;

	// check if there're enough buffers available...
	for (i = 0, buf = ctx->buffers; i < AL_MAXBUFFERS; i++, buf++)
	{
		if (!buf->inUse)
			iCount++;
	} // for
	
	if (iCount < n)
		__alSetError(AL_INVALID_VALUE);
    else
	{
		iCount = 0;
	    for (i = 0, buf = ctx->buffers; i < AL_MAXBUFFERS; i++, buf++)
		{
			if (!buf->inUse)
			{
                __alBuffersInit(buf, 1);
                buf->inUse = AL_TRUE;
				buffers[iCount] = i + 1;  // see comments about the plus 1.
				iCount++;
    			if (iCount >= n)
                    break;
			} // if
		} // for
	} // else

    __alUngrabContext(ctx);
} // alGenBuffers


ALAPI ALvoid ALAPIENTRY alDeleteBuffers(ALsizei n, ALuint *buffers)
{
	register int i = 0;
    //register int j = 0;
	register ALboolean bInvalid = AL_FALSE;
    register ALbuffer *buf;
    register ALcontext *ctx;

	if (n == 0)
		return; // legal no-op.

	else if (n < 0)
    {
		__alSetError(AL_INVALID_VALUE);
		return;
	} // else if

    ctx = __alGrabCurrentContext();
    if (ctx == NULL)
        return;

	// make sure none of the buffers are attached to a source
    buf = ctx->buffers;

#if 0  // !!! FIXME: Doesn't account for queued buffers.
	for (i = 0; i < n; i++)
	{
	    for (j = 0; j < AL_MAXSOURCES; j++)
		{
		    if (gSource[j].srcBufferNum == buffers[i])
			{
			    bAttached = AL_TRUE;
				break;
			} // if
		} // for

		if (bAttached == AL_TRUE)
		    break;

        buf++;
	} // for
#else
//printf("OPENAL FIXME: deleting buffers without checking source attachment!\n");
#endif

    // passed all tests, so do the deletion...
	if (bInvalid == AL_FALSE)
	{
        register ALuint name;
	    for (i = 0; i < n; i++)
		{
            name = buffers[i] - 1; // minus one to account for Buffer Zero.
			if ((name < AL_MAXBUFFERS) && (buf[name].inUse))
			{
                __alBuffersShutdown(buf + name, 1);
                buf[name].inUse = AL_FALSE;
	 		} // if
		} // for
	} // if

	if (bInvalid == AL_TRUE)
		__alSetError(AL_INVALID_VALUE);

    __alUngrabContext(ctx);
} // alDeleteBuffers


ALAPI ALboolean ALAPIENTRY alIsBuffer(ALuint buffer)
{
    ALbuffer *buf;
    register ALcontext *ctx = __alGrabContextAndGetBuffer(buffer, &buf);

    // ctx will be null if there's no context or the buffer name is bogus.
    if (ctx == NULL)
        return AL_FALSE;

    __alUngrabContext(ctx);
	return AL_TRUE;
} // alIsBuffer


// !!! FIXME: Make these resamplers not suck.
// !!! FIXME:  ... and do altivec versions of them, too.

static ALboolean __alBufferDataFromStereo8(ALcontext *ctx, ALbuffer *buf,
                                           ALvoid *_src, ALsizei size,
                                           ALsizei freq)
{
    register ALfloat devRate = (ALfloat) (ctx->device->streamFormat.mSampleRate);
    register ALfloat ratio;
    register UInt8 *dst = (UInt8 *) buf->mixData;
    register UInt8 *max = dst + (buf->allocatedSpace >> 1);
    register UInt8 *src = (UInt8 *) _src;
    register SInt32 sampL;
    register SInt32 sampR;
    register SInt32 lastSampL;
    register SInt32 lastSampR;
    register UInt8 avgL;
    register UInt8 avgR;
    register int incr;
    register int maxincr;

    // resample to device frequency...
    ratio = ((ALfloat) freq) / ((ALfloat) devRate);
    lastSampL = ((SInt32) src[0]) - 128;
    lastSampR = ((SInt32) src[1]) - 128;

    if (ratio < 1.0f)  // upsampling.
    {
        maxincr = (int) (1.0f / ratio);

        if (maxincr == 2)  // fast path for doubling rate.
        {
            while (dst != max)
            {
                sampL = ((SInt32) src[0]) - 128;
                sampR = ((SInt32) src[1]) - 128;
                src += 2;
                avgL = ((sampL + lastSampL) >> 1);
                avgR = ((sampR + lastSampR) >> 1);
                lastSampL = sampL;
                lastSampR = sampR;
                dst[0] = avgL;
                dst[1] = avgR;
                dst[2] = avgL;
                dst[3] = avgR;
                dst += 4;
            } // while
        } // if

        else if (maxincr == 4)  // fast path for quadrupling rate.
        {
            while (dst != max)   // !!! FIXME: Altivec!
            {
                sampL = (SInt32) src[0];
                sampR = (SInt32) src[1];
                src += 2;
                avgL = ((sampL + lastSampL) >> 1);
                avgR = ((sampR + lastSampR) >> 1);
                lastSampL = sampL;
                lastSampR = sampR;
                dst[0] = avgL;
                dst[1] = avgR;
                dst[2] = avgL;
                dst[3] = avgR;
                dst[4] = avgL;
                dst[5] = avgR;
                dst[6] = avgL;
                dst[7] = avgR;
                dst += 8;
            } // while
        } // if

        else  // arbitrary upsampling.
        {
            while (dst != max)
            {
                sampL = (SInt32) src[0];
                sampR = (SInt32) src[1];
                src += 2;
                avgL = ((sampL + lastSampL) >> 1);
                avgR = ((sampR + lastSampR) >> 1);
                lastSampL = sampL;
                lastSampR = sampR;
                for (incr = 0; (incr < maxincr) && (dst < max); incr++, dst+=2)
                {
                    dst[0] = avgL;
                    dst[1] = avgR;
                } // for
            } // while
        } // else
    } // if

    else  // downsampling
    {
        maxincr = (((int) ratio) << 1);

        while (dst != max)
        {
            sampL = (SInt32) src[0];
            sampR = (SInt32) src[1];
            src += 2;
            dst[0] = ((sampL + lastSampL) >> 1);
            dst[1] = ((sampR + lastSampR) >> 1);
            lastSampL = sampL;
            lastSampR = sampR;
            dst += 2;
        } // while
    } // else

    return(AL_TRUE);
} // __alBufferDataFromStereo8


static ALboolean __alBufferDataFromStereo16(ALcontext *ctx, ALbuffer *buf,
                                            ALvoid *_src, ALsizei size,
                                            ALsizei freq)
{
    register ALfloat devRate = (ALfloat) (ctx->device->streamFormat.mSampleRate);
    register ALfloat ratio;
    register SInt16 *dst = (SInt16 *) buf->mixData;
    register SInt16 *max = dst + (buf->allocatedSpace >> 1);
    register SInt16 *src = (SInt16 *) _src;
    register SInt32 sampL;
    register SInt32 sampR;
    register SInt32 lastSampL;
    register SInt32 lastSampR;
    register SInt16 avgL;
    register SInt16 avgR;
    register int incr;
    register int maxincr;

    // resample to device frequency...
    ratio = ((ALfloat) freq) / ((ALfloat) devRate);
    lastSampL = (SInt32) src[0];
    lastSampR = (SInt32) src[1];

    if (ratio < 1.0f)  // upsampling.
    {
        maxincr = (int) (1.0f / ratio);

        if (maxincr == 2)  // fast path for doubling rate.
        {
            while (dst != max)
            {
                sampL = (SInt32) src[0];
                sampR = (SInt32) src[1];
                src += 2;
                avgL = ((sampL + lastSampL) >> 1);
                avgR = ((sampR + lastSampR) >> 1);
                lastSampL = sampL;
                lastSampR = sampR;
                dst[0] = avgL;
                dst[1] = avgR;
                dst[2] = avgL;
                dst[3] = avgR;
                dst += 4;
            } // while
        } // if

        else if (maxincr == 4)  // fast path for quadrupling rate.
        {
            while (dst != max)   // !!! FIXME: Altivec!
            {
                sampL = (SInt32) src[0];
                sampR = (SInt32) src[1];
                src += 2;
                avgL = ((sampL + lastSampL) >> 1);
                avgR = ((sampR + lastSampR) >> 1);
                lastSampL = sampL;
                lastSampR = sampR;
                dst[0] = avgL;
                dst[1] = avgR;
                dst[2] = avgL;
                dst[3] = avgR;
                dst[4] = avgL;
                dst[5] = avgR;
                dst[6] = avgL;
                dst[7] = avgR;
                dst += 8;
            } // while
        } // if

        else  // arbitrary upsampling.
        {
            while (dst != max)
            {
                sampL = (SInt32) src[0];
                sampR = (SInt32) src[1];
                src += 2;
                avgL = ((sampL + lastSampL) >> 1);
                avgR = ((sampR + lastSampR) >> 1);
                lastSampL = sampL;
                lastSampR = sampR;
                for (incr = 0; (incr < maxincr) && (dst < max); incr++, dst+=2)
                {
                    dst[0] = avgL;
                    dst[1] = avgR;
                } // for
            } // while
        } // else
    } // if

    else  // downsampling
    {
        maxincr = (((int) ratio) << 1);

        while (dst != max)
        {
            sampL = (SInt32) src[0];
            sampR = (SInt32) src[1];
            src += 2;
            dst[0] = ((sampL + lastSampL) >> 1);
            dst[1] = ((sampR + lastSampR) >> 1);
            lastSampL = sampL;
            lastSampR = sampR;
            dst += 2;
        } // while
    } // else

    return(AL_TRUE);
} // __alBufferDataFromStereo16



static ALboolean __alBufferDataFromMono8(ALcontext *ctx, ALbuffer *buf,
                                         ALvoid *_src, ALsizei size,
                                         ALsizei freq)
{
    register ALfloat devRate = (ALfloat) (ctx->device->streamFormat.mSampleRate);
    register ALfloat ratio;
    register SInt8 *dst = (SInt8 *) buf->mixData;
    register SInt8 *max = dst + buf->allocatedSpace;
    register UInt8 *src = (UInt8 *) _src;
    register SInt32 samp;
    register SInt32 lastSamp;
    register SInt8 avg;
    register int incr;
    register int maxincr;

    // resample to device frequency...
    ratio = ((ALfloat) freq) / ((ALfloat) devRate);
    lastSamp = ((SInt32) *src) - 128; // -128 to convert to signed.

    if (ratio <= 1.0f)  // upsampling.
    {
        maxincr = (int) (1.0f / ratio);

        if (maxincr == 1)  // fast path for just converting to signed.
        {
            while (dst != max)
            {
                // !!! FIXME: Altivec!
                // !!! FIXME: Do this at least 32 bits at a time.
                samp = ((SInt32) *src) - 128; // -128 to convert to signed.
                src++;
                *dst = (SInt8) samp;
                dst++;
            } // while
        } // if

        if (maxincr == 2)  // fast path for doubling rate.
        {
            while (dst != max)
            {
                samp = ((SInt32) *src) - 128; // -128 to convert to signed.
                src++;
                avg = ((samp + lastSamp) >> 1);
                lastSamp = samp;
                dst[0] = avg;
                dst[1] = avg;
                dst += 2;
            } // while
        } // if

        else if (maxincr == 4)  // fast path for quadrupling rate.
        {
            while (dst != max)
            {
                samp = ((SInt32) *src) - 128;  // -128 to convert to signed.
                src++;
                avg = ((samp + lastSamp) >> 1);
                lastSamp = samp;
                dst[0] = avg;
                dst[1] = avg;
                dst[2] = avg;
                dst[3] = avg;
                dst += 4;
            } // while
        } // if

        else  // arbitrary upsampling.
        {
            while (dst != max)
            {
                samp = ((SInt32) *src) - 128;  // -128 to convert to signed.
                src++;
                avg = ((samp + lastSamp) >> 1);
                lastSamp = samp;
                for (incr = 0; (incr < maxincr) && (dst < max); incr++, dst++)
                    *dst = avg;
            } // while
        } // else
    } // if

    else  // downsampling
    {
        maxincr = (int) ratio;

        while (dst != max)
        {
            samp = ((SInt32) *src) - 128;  // -128 to convert to signed.
            src += maxincr;
            *dst = ((samp + lastSamp) >> 1);
            lastSamp = samp;
            dst++;
        } // while
    } // else

    return(AL_TRUE);
} // __alBufferDataFromMono8


static ALboolean __alBufferDataFromMono16(ALcontext *ctx, ALbuffer *buf,
                                          ALvoid *_src, ALsizei size,
                                          ALsizei freq)
{
    register ALfloat devRate = (ALfloat) (ctx->device->streamFormat.mSampleRate);
    register ALfloat ratio;
    register SInt16 *dst = (SInt16 *) buf->mixData;
    register SInt16 *max = dst + (buf->allocatedSpace >> 1);
    register SInt16 *src = (SInt16 *) _src;
    register SInt32 samp;
    register SInt32 lastSamp;
    register SInt16 avg;
    register int incr;
    register int maxincr;

    // resample to device frequency...
    ratio = ((ALfloat) freq) / ((ALfloat) devRate);
    lastSamp = (SInt32) *src;

    if (ratio < 1.0f)  // upsampling.
    {
        maxincr = (int) (1.0f / ratio);

        if (maxincr == 2)  // fast path for doubling rate.
        {
            while (dst != max)
            {
                samp = (SInt32) *src;
                src++;
                avg = ((samp + lastSamp) >> 1);
                lastSamp = samp;
                dst[0] = avg;
                dst[1] = avg;
                dst += 2;
            } // while
        } // if

        else if (maxincr == 4)  // fast path for quadrupling rate.
        {
            while (dst != max)
            {
                samp = (SInt32) *src;
                src++;
                avg = ((samp + lastSamp) >> 1);
                lastSamp = samp;
                dst[0] = avg;
                dst[1] = avg;
                dst[2] = avg;
                dst[3] = avg;
                dst += 4;
            } // while
        } // if

        else  // arbitrary upsampling.
        {
            while (dst != max)
            {
                samp = (SInt32) *src;
                src++;
                avg = ((samp + lastSamp) >> 1);
                lastSamp = samp;
                for (incr = 0; (incr < maxincr) && (dst < max); incr++, dst++)
                    *dst = avg;
            } // while
        } // else
    } // if

    else  // downsampling
    {
        maxincr = (int) ratio;

        while (dst != max)
        {
            samp = (SInt32) *src;
            src += maxincr;
            *dst = ((samp + lastSamp) >> 1);
            lastSamp = samp;
            dst++;
        } // while
    } // else

    return(AL_TRUE);
} // __alBufferDataFromMono16


// Use this when no conversion is needed.
static ALboolean __alBufferDataSimpleMemcpy(ALcontext *ctx, ALbuffer *buf,
                                            ALvoid *_src, ALsizei size,
                                            ALsizei freq)
{
    // !!! FIXME: Use vm_copy?
    memcpy(buf->mixData, _src, buf->allocatedSpace);
    return(AL_TRUE);
} // __alBufferDataSimpleMemcpy


typedef ALboolean (*bufcvt_t)(ALcontext*,ALbuffer*,ALvoid*,ALsizei,ALsizei);

static ALboolean __alDoBufferConvert(ALcontext *ctx, ALbuffer *buffer,
                                     ALenum format, ALvoid *data,
                                     ALsizei size, ALsizei freq)
{
    ALfloat devRate = (ALfloat) (ctx->device->streamFormat.mSampleRate);
    ALint channels = (ALint) (ctx->device->streamFormat.mChannelsPerFrame);
	ALfloat ratio = devRate / ((ALfloat) freq);
    bufcvt_t fn = NULL;
    ALsizei newAlloc = 0;
    ALboolean alwaysDoConvert = AL_FALSE;

    // we resample to device frequency at dereference time.
    buffer->frequency = (ALsizei) devRate;
    buffer->format = format;  // ... but keep the original format.
    buffer->mixFunc = NULL;

    switch (format)
	{
        case AL_FORMAT_STEREO8:
            //printf("buffering stereo8 data...\n");
            fn = __alBufferDataFromStereo8;
            alwaysDoConvert = AL_TRUE;  // must convert to signed.
            newAlloc = ((ALsizei) ((size >> 1) * ratio)) << 1;
            buffer->bits = 8;
            buffer->channels = 2;
            buffer->compressed = AL_FALSE;
            buffer->mixFunc = __alMixStereo8;
		    break;

		case AL_FORMAT_MONO8 :
            //printf("buffering mono8 data...\n");
            fn = __alBufferDataFromMono8;
            alwaysDoConvert = AL_TRUE;  // must convert to signed.
            newAlloc = (ALsizei) (size * ratio);
            buffer->bits = 8;
            buffer->channels = 1;
            buffer->compressed = AL_FALSE;
            buffer->mixFunc = __alMixMono8;
		    break;

		case AL_FORMAT_STEREO16:
            //printf("buffering stereo16 data...\n");
            fn = __alBufferDataFromStereo16;
            newAlloc = ((ALsizei) ((size >> 2) * ratio)) << 2;
            buffer->bits = 16;
            buffer->channels = 2;
            buffer->compressed = AL_FALSE;
            if ((__alHasEnabledVectorUnit) && (channels == 2))
                buffer->mixFunc = __alMixStereo16_altivec;
            else
                buffer->mixFunc = __alMixStereo16;
		    break;

		case AL_FORMAT_MONO16:
            //printf("buffering mono16 data...\n");
            fn = __alBufferDataFromMono16;
            newAlloc = ((ALsizei) ((size >> 1) * ratio)) << 1;
            buffer->bits = 16;
            buffer->channels = 1;
            buffer->compressed = AL_FALSE;
            if ((__alHasEnabledVectorUnit) && (channels == 2))
                buffer->mixFunc = __alMixMono16_altivec;
            else
                buffer->mixFunc = __alMixMono16;
		    break;

        #if SUPPORTS_AL_EXT_VORBIS
        case AL_FORMAT_VORBIS_EXT:
            //printf("buffering vorbis data...\n");
            fn = __alBufferDataFromVorbis;
            alwaysDoConvert = AL_TRUE;  // obviously, this is specialized.
            newAlloc = size;
            buffer->bits = 32;
            buffer->channels = 2;
            buffer->compressed = AL_TRUE;
            break;
        #endif

		default:
            //printf("tried to buffer %d data.\n", format);
			__alSetError(AL_ILLEGAL_ENUM);
	} // switch

    // Fast path if no sample rate conversion needed...
    if ( (((ALsizei) devRate) == freq) && (!alwaysDoConvert) )
    {
        //printf("  ...Buffering with simple memcpy...\n");
        newAlloc = size;
        fn = __alBufferDataSimpleMemcpy;
    } // if

    if (newAlloc != buffer->allocatedSpace)
    {
        void *ptr = realloc(buffer->mixUnalignedPtr, newAlloc + 16);
        if (ptr == NULL)
        {
            __alSetError(AL_OUT_OF_MEMORY);
            return(AL_FALSE);
        } // if
        buffer->allocatedSpace = newAlloc;
        buffer->mixUnalignedPtr = ptr;
        buffer->mixData = ptr + (16 - (((size_t) ptr) % 16));
    } // if

    return(fn(ctx, buffer, data, size, freq));
} // __alDoBufferConvert


ALAPI ALvoid ALAPIENTRY alBufferData(ALuint buffer,ALenum format,ALvoid *data,ALsizei size,ALsizei freq)
{
    ALbuffer *buf;
    register ALcontext *ctx = __alGrabContextAndGetBuffer(buffer, &buf);
    if (ctx == NULL)
        return;

    // !!! FIXME: Make sure we're not in a source's buffer queue.

    //printf("alBufferData() bid==%d, size==%d\n", (int) buffer, (int) size);

    if (buf->deleteBuffer)
        buf->deleteBuffer(buf);

    __alDoBufferConvert(ctx, buf, format, data, size, freq);
    __alUngrabContext(ctx);
} // alBufferData


ALAPI ALvoid ALAPIENTRY alGetBufferf(ALuint buffer, ALenum pname, ALfloat *value)
{
    ALbuffer *buf;
    register ALcontext *ctx = __alGrabContextAndGetBuffer(buffer, &buf);
    if (ctx == NULL)
        return;

	switch(pname)
	{
        case AL_FREQUENCY:
            *value=((ALfloat) ctx->device->streamFormat.mSampleRate);
            break;
		default:
		    __alSetError(AL_INVALID_ENUM);
	} // switch

    __alUngrabContext(ctx);
} // alGetBufferf


ALAPI ALvoid ALAPIENTRY alGetBufferfv(ALuint buffer, ALenum pname, ALfloat *value)
{
    alGetBufferf(buffer, pname, value);
} // alGetBufferfv


ALAPI ALvoid ALAPIENTRY alGetBufferi(ALuint buffer, ALenum pname, ALint *value)
{
    ALbuffer *buf;
    register ALcontext *ctx = __alGrabContextAndGetBuffer(buffer, &buf);
    if (ctx == NULL)
        return;

    switch(pname)
    {
        case AL_FREQUENCY:
            *value=((ALint) ctx->device->streamFormat.mSampleRate);
            break;
        case AL_BITS:
            *value=buf->bits;
            break;
        case AL_CHANNELS:
            *value=buf->channels;
            break;
        case AL_SIZE:
            *value=buf->allocatedSpace;
            break;
        default:
            __alSetError(AL_INVALID_ENUM);
    } // switch

    __alUngrabContext(ctx);
} // alGetBufferi


ALAPI ALvoid ALAPIENTRY alGetBufferiv(ALuint buffer, ALenum pname, ALint *value)
{
    alGetBufferi(buffer, pname, value);
} // alGetBufferiv


// end of alBuffer.c ...

