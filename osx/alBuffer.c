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
// !!! FIXME:  ... and move them out of alBuffer.c.
// !!! FIXME:  ... and do altivec versions of them, too.

ALboolean __alResampleStereo8(ALvoid *_src, ALsizei _srcsize,
                              ALvoid *_dst, ALsizei _dstsize)
{
    register ALfloat ratio = ((ALfloat) _dstsize) / ((ALfloat) _srcsize);
    register UInt8 *dst = (UInt8 *) _dst;
    register UInt8 *max = dst + _dstsize;
    register UInt8 *src = (UInt8 *) _src;
    register SInt32 lastSampL = ((SInt32) src[0]) - 128;
    register SInt32 lastSampR = ((SInt32) src[1]) - 128;
    register SInt32 sampL;
    register SInt32 sampR;

    // resample to device frequency...

    if (ratio > 1.0f)  // upsampling.
    {
        if (EPSILON_EQUAL(ratio, 2.0f)) // fast path for doubling rate.
        {
            while (dst != max)
            {
                //linear interp
                sampL = ((SInt32) src[0]) - 128;
                sampR = ((SInt32) src[1]) - 128;
                src += 2;
                dst[0] = ((sampL + lastSampL) >> 1);
                dst[1] = ((sampR + lastSampR) >> 1);
                dst[2] = sampL;
                dst[3] = sampR;
                lastSampL = sampL;
                lastSampR = sampR;
                dst += 4;
            } // while
        } // if

        if (EPSILON_EQUAL(ratio, 4.0f)) // fast path for quadrupling rate.
        {
            while (dst != max)   // !!! FIXME: Altivec!
            {
                sampL = ((SInt32) src[0])-128;
                sampR = ((SInt32) src[1])-128;
                src += 2;
                dst[0] = ((sampL + 3*lastSampL) >> 2);
                dst[1] = ((sampR + 3*lastSampR) >> 2);
                dst[2] = ((sampL + lastSampL) >> 1);
                dst[3] = ((sampR + lastSampR) >> 1);
                dst[4] = ((3*sampL + lastSampL) >> 2);
                dst[5] = ((3*sampR + lastSampR) >> 2);
                dst[6] = sampL;
                dst[7] = sampR;
                lastSampL = sampL;
                lastSampR = sampR;
                dst += 8;
            } // while
        } // if

        else  // arbitrary upsampling.
        {
            #if 1
            // Arbitrary upsampling based on Bresenham's line algorithm.
            // !!! FIXME: Needs better interpolation.
            // !!! FIXME: Replace with "run lengths".
            register int dstsize = _dstsize;
            register int eps = 0;
            _srcsize -= 4;  // fudge factor.
            sampL = (SInt32) src[0];
            sampR = (SInt32) src[1];
            while (dst != max)
            {
                dst[0] = sampL;
                dst[1] = sampR;
                dst += 2;
                eps += _srcsize;
                if ((eps << 1) >= dstsize)
                {
                    src += 2;
                    sampL = (src[0] + lastSampL) >> 1;
                    sampR = (src[1] + lastSampR) >> 1;
                    lastSampL = sampL;
                    lastSampR = sampR;
                    eps -= dstsize;
                } // if
            } // while
            #else
            // Arbitrary upsampling based on Abrash's interpretation of
            //  Bresenham's run length sliced line rendering algorithm.
            // !!! FIXME: Needs better interpolation.
            register int dstsize = _dstsize;
            register int wholestep = dstsize / _srcsize;
            register int adjup = (dstsize % _srcsize);
            register int adjdown = size << 1;
            register int errorterm = adjup - adjdown;
            register int initialsampcount = (wholestep >> 1);
            register int finalsampcount = initialsampcount;
            register int runlength;
            register SInt8 samp8L;
            register SInt8 samp8R;

            max -= finalsampcount << 1;
            adjup <<= 1;
            if (wholestep & 0x01)
                errorterm += size;
            else
            {
                if (adjup == 0)
                    initialsampcount--;
            } // else

            #define DO_RESAMPLING_RUNLENGTH_MACRO_STEREO8(x) \
                sampL = ( (((SInt32) src[0]) - 128) + lastSampL) >> 1; \
                sampR = ( (((SInt32) src[1]) - 128) + lastSampR) >> 1; \
                samp8L = (SInt8) sampL; \
                samp8R = (SInt8) sampR; \
                lastSampL = sampL; \
                lastSampR = sampR; \
                while (x--) { dst[0] = samp8L; dst[1] = samp8R; dst += 2; } \
                src += 2;

            DO_RESAMPLING_RUNLENGTH_MACRO_STEREO8(initialsampcount);
            while (dst != max)
            {
                runlength = wholestep;
                if ((errorterm += adjup) > 0)
                {
                    runlength++;
                    errorterm -= adjdown;
                } // if
                DO_RESAMPLING_RUNLENGTH_MACRO_STEREO8(runlength);
            } // while
            DO_RESAMPLING_RUNLENGTH_MACRO_STEREO8(finalsampcount);
            #endif
        } // else
    } // if

    else  // downsampling
    {
        #if 1
        // Arbitrary downsampling based on Bresenham's line algorithm.
        // !!! FIXME: Needs better interpolation.
        register int dstsize = _dstsize;
        register int srcsize = _srcsize;
        register int eps = 0;
        srcsize -= 4;  // fudge factor.
        lastSampL = sampL = (SInt32) src[0];
        lastSampR = sampR = (SInt32) src[1];
        while (dst != max)
        {
            src += 2;
            eps += dstsize;
            if ((eps << 1) >= srcsize)
            {
                dst[0] = sampL;
                dst[1] = sampR;
                dst += 2;
                sampL = ( (((SInt32) src[0]) - 128) + lastSampL ) >> 1;
                sampR = ( (((SInt32) src[1]) - 128) + lastSampR ) >> 1;
                lastSampL = sampL;
                lastSampR = sampR;
                eps -= srcsize;
            } // if
        } // while
        #else
        // Arbitrary downsampling based on Abrash's interpretation of
        //  Bresenham's run length sliced line rendering algorithm.
        // !!! FIXME: Needs better interpolation.
        register int dstsize = _dstsize;
        register int wholestep = _srcsize / dstsize;
        register int adjup = (_srcsize % dstsize);
        register int adjdown = dstsize << 1;
        register int errorterm = adjup - adjdown;
        register int initialsampcount = (wholestep >> 1);

        adjup <<= 1;
        if (wholestep & 0x01)
            errorterm += size;
        else
        {
            if (adjup == 0)
                initialsampcount--;
        } // else

        *((SInt16 *) dst) = *((SInt16 *) src);  // copy first two samp points.
        dst += 2;
        src += initialsampcount << 1;
        while (dst != max)
        {
            if ((errorterm += adjup) <= 0)
                src += wholestep;
            else
            {
                src += wholestep + 1;
                errorterm -= adjdown;
            } // else

            sampL = ( (((SInt32) src[0]) - 128) + lastSampL ) >> 1;
            sampR = ( (((SInt32) src[1]) - 128) + lastSampR ) >> 1;
            dst[0] = (SInt8) sampL;
            dst[1] = (SInt8) sampR;
            dst += 2;
            lastSampL = sampL;
            lastSampR = sampR;
        } // while
        #endif
    } // else

    return(AL_TRUE);
} // __alResampleStereo8


ALboolean __alResampleStereo16(ALvoid *_src, ALsizei _srcsize,
                               ALvoid *_dst, ALsizei _dstsize)
{
    register ALfloat ratio = ((ALfloat) _dstsize) / ((ALfloat) _srcsize);
    register SInt16 *dst = (SInt16 *) _dst;
    register SInt16 *max = dst + (_dstsize >> 1);
    register SInt16 *src = (SInt16 *) _src;
    register SInt32 lastSampL = (SInt32) src[0];
    register SInt32 lastSampR = (SInt32) src[1];
    register SInt32 sampL;
    register SInt32 sampR;

    // resample to device frequency...

    if (ratio > 1.0f)  // upsampling.
    {
        if (EPSILON_EQUAL(ratio, 2.0f)) // fast path for doubling rate.
        {
            while (dst != max)
            {
                sampL = (SInt32) src[0];
                sampR = (SInt32) src[1];
                src += 2;
                dst[0] = ((sampL + lastSampL) >> 1);
                dst[1] = ((sampR + lastSampR) >> 1);
                dst[2] = sampL;
                dst[3] = sampR;
                lastSampL = sampL;
                lastSampR = sampR;
                dst += 4;
            } // while
        } // if

        else if (EPSILON_EQUAL(ratio, 4.0f)) // fast path for quadrupling rate.
        {
            while (dst != max)   // !!! FIXME: Altivec!
            {
                sampL = (SInt32) src[0];
                sampR = (SInt32) src[1];
                src += 2;
                dst[0] = ((sampL + 3*lastSampL) >> 2);
                dst[1] = ((sampR + 3*lastSampR) >> 2);
                dst[2] = ((sampL + lastSampL) >> 1);
                dst[3] = ((sampR + lastSampR) >> 1);
                dst[4] = ((3*sampL + lastSampL) >> 2);
                dst[5] = ((3*sampR + lastSampR) >> 2);
                dst[6] = sampL;
                dst[7] = sampR;
                lastSampL = sampL;
                lastSampR = sampR;
                dst += 8;
            } // while
        } // if

        else  // arbitrary upsampling.
        {
            #if 1
            // Arbitrary upsampling based on Bresenham's line algorithm.
            // !!! FIXME: Needs better interpolation.
            register int dstsize = _dstsize;
            register int srcsize = _srcsize;
            register int eps = 0;
            srcsize -= 8;  // fudge factor.
            sampL = (SInt32) src[0];
            sampR = (SInt32) src[1];
            while (dst != max)
            {
                dst[0] = sampL;
                dst[1] = sampR;
                dst += 2;
                eps += srcsize;
                if ((eps << 1) >= dstsize)
                {
                    src += 2;
                    sampL = (src[0] + lastSampL) >> 1;
                    sampR = (src[1] + lastSampR) >> 1;
                    lastSampL = sampL;
                    lastSampR = sampR;
                    eps -= dstsize;
                } // if
            } // while
            #else
            // Arbitrary upsampling based on Abrash's interpretation of
            //  Bresenham's run length sliced line rendering algorithm.
            // !!! FIXME: Needs better interpolation.
            register int dstsize = _dstsize;
            register int wholestep = dstsize / _srcsize;
            register int adjup = (dstsize % _srcsize);
            register int adjdown = size << 1;
            register int errorterm = adjup - adjdown;
            register int initialsampcount = (wholestep >> 1);
            register int finalsampcount = initialsampcount;
            register int runlength;
            register SInt16 samp16L;
            register SInt16 samp16R;

            max -= finalsampcount << 1;
            adjup <<= 1;
            if (wholestep & 0x01)
                errorterm += size;
            else
            {
                if (adjup == 0)
                    initialsampcount--;
            } // else

            #define DO_RESAMPLING_RUNLENGTH_MACRO_STEREO16(x) \
                sampL = ( ((SInt32) src[0]) + lastSampL ) >> 1; \
                sampR = ( ((SInt32) src[1]) + lastSampR ) >> 1; \
                samp16L = (SInt16) sampL; \
                samp16R = (SInt16) sampR; \
                lastSampL = sampL; \
                lastSampR = sampR; \
                while (x--) { dst[0] = samp16L; dst[1] = samp16R; dst += 2; } \
                src += 2;

            DO_RESAMPLING_RUNLENGTH_MACRO_STEREO16(initialsampcount);
            while (dst != max)
            {
                runlength = wholestep;
                if ((errorterm += adjup) > 0)
                {
                    runlength++;
                    errorterm -= adjdown;
                } // if
                DO_RESAMPLING_RUNLENGTH_MACRO_STEREO16(runlength);
            } // while
            DO_RESAMPLING_RUNLENGTH_MACRO_STEREO16(finalsampcount);
            #endif
        } // else
    } // if

    else  // downsampling
    {
        #if 1
        // Arbitrary downsampling based on Bresenham's line algorithm.
        // !!! FIXME: Needs better interpolation.
        // !!! FIXME: Replace with "run lengths".
        register int dstsize = _dstsize;
        register int srcsize = _srcsize;
        register int eps = 0;
        srcsize -= 8;  // fudge factor.
        lastSampL = sampL = (SInt32) src[0];
        lastSampR = sampR = (SInt32) src[1];
        while (dst != max)
        {
            src += 2;
            eps += dstsize;
            if ((eps << 1) >= srcsize)
            {
                dst[0] = sampL;
                dst[1] = sampR;
                dst += 2;
                sampL = (src[0] + lastSampL) >> 1;
                sampR = (src[1] + lastSampR) >> 1;
                lastSampL = sampL;
                lastSampR = sampR;
                eps -= srcsize;
            } // if
        } // while
        #else
        // Arbitrary downsampling based on Abrash's interpretation of
        //  Bresenham's run length sliced line rendering algorithm.
        // !!! FIXME: Needs better interpolation.
        register int dstsize = _dstsize;
        register int wholestep = _srcsize / dstsize;
        register int adjup = (_srcsize % dstsize);
        register int adjdown = dstsize << 1;
        register int errorterm = adjup - adjdown;
        register int initialsampcount = (wholestep >> 1);

        adjup <<= 1;
        if (wholestep & 0x01)
            errorterm += size;
        else
        {
            if (adjup == 0)
                initialsampcount--;
        } // else

        *((SInt32 *) dst) = *((SInt32 *) src);  // copy first two samp points.
        dst += 2;
        src += initialsampcount << 1;
        while (dst != max)
        {
            if ((errorterm += adjup) <= 0)
                src += wholestep;
            else
            {
                src += wholestep + 1;
                errorterm -= adjdown;
            } // else

            sampL = ( ((SInt32) src[0]) + lastSampL ) >> 1;
            sampR = ( ((SInt32) src[1]) + lastSampR ) >> 1;
            dst[0] = (SInt16) sampL;
            dst[1] = (SInt16) sampR;
            dst += 2;
            lastSampL = sampL;
            lastSampR = sampR;
        } // while
        #endif
    } // else

    return(AL_TRUE);
} // __alResampleStereo16


ALboolean __alResampleMono8(ALvoid *_src, ALsizei _srcsize,
                            ALvoid *_dst, ALsizei _dstsize)
{
    register ALfloat ratio = ((ALfloat) _dstsize) / ((ALfloat) _srcsize);
    register SInt8 *dst = (SInt8 *) _dst;
    register SInt8 *max = dst + _dstsize;
    register UInt8 *src = (UInt8 *) _src;
    register SInt32 lastSamp = ((SInt32) *src) - 128; // -128 to convert to signed.
    register SInt32 samp;

    // resample to device frequency...

    if (ratio > 1.0f)  // upsampling.
    {
        if (EPSILON_EQUAL(ratio, 1.0f)) // fast path for signed conversion.
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

        else if (EPSILON_EQUAL(ratio, 2.0f)) // fast path for doubling rate.
        {
            while (dst != max)
            {
                samp = ((SInt32) *src) - 128; // -128 to convert to signed.
                src++;
                dst[0] = ((samp + lastSamp) >> 1);
                dst[1] = samp;
                lastSamp = samp;
                dst += 2;
            } // while
        } // if

        else if (EPSILON_EQUAL(ratio, 4.0f)) // fast path for quadrupling rate.
        {
            while (dst != max)
            {
                samp = ((SInt32) *src) - 128;  // -128 to convert to signed.
                src++;
                dst[0] = ((samp + 3*lastSamp) >> 2);
                dst[1] = ((samp + lastSamp) >> 1);
                dst[2] = ((3*samp + lastSamp) >> 2);
                dst[3] = samp;
                dst += 4;
                lastSamp = samp;
            } // while
        } // if

        else  // arbitrary upsampling.
        {
            #if 1
            // Arbitrary upsampling based on Bresenham's line algorithm.
            // !!! FIXME: Needs better interpolation.
            // !!! FIXME: Replace with "run lengths".
            register int dstsize = _dstsize;
            register int srcsize = _srcsize;
            register int eps = 0;
            srcsize -= 2;  // fudge factor.
            samp = (SInt32) *src;
            while (dst != max)
            {
                *dst = samp;
                dst++;
                eps += srcsize;
                if ((eps << 1) >= dstsize)
                {
                    src++;
                    samp = ( (((SInt32) *src) - 128) + lastSamp ) >> 1;
                    lastSamp = samp;
                    eps -= dstsize;
                } // if
            } // while
            #else
            // Arbitrary upsampling based on Abrash's interpretation of
            //  Bresenham's run length sliced line rendering algorithm.
            // !!! FIXME: Needs better interpolation.
            register int dstsize = _dstsize;
            register int wholestep = dstsize / _srcsize;
            register int adjup = (dstsize % _srcsize);
            register int adjdown = size << 1;
            register int errorterm = adjup - adjdown;
            register int initialsampcount = (wholestep >> 1);
            register int finalsampcount = initialsampcount;
            register int runlength;
            register SInt8 samp8;

            max -= finalsampcount;
            adjup <<= 1;
            if (wholestep & 0x01)
                errorterm += size;
            else
            {
                if (adjup == 0)
                    initialsampcount--;
            } // else

            #define DO_RESAMPLING_RUNLENGTH_MACRO_MONO8(x) \
                samp = ( (((SInt32) *src) - 128) + lastSamp ) >> 1; \
                samp8 = (SInt8) samp; \
                lastSamp = samp; \
                while (x--) { *dst = samp8; dst++; } \
                src++;

            DO_RESAMPLING_RUNLENGTH_MACRO_MONO8(initialsampcount);
            while (dst != max)
            {
                runlength = wholestep;
                if ((errorterm += adjup) > 0)
                {
                    runlength++;
                    errorterm -= adjdown;
                } // if
                DO_RESAMPLING_RUNLENGTH_MACRO_MONO8(runlength);
            } // while
            DO_RESAMPLING_RUNLENGTH_MACRO_MONO8(finalsampcount);
            #endif
        } // else
    } // if

    else  // downsampling
    {
        #if 1
        // Arbitrary downsampling based on Bresenham's line algorithm.
        // !!! FIXME: Needs better interpolation.
        // !!! FIXME: Replace with "run lengths".
        register int dstsize = _dstsize;
        register int srcsize = _srcsize;
        register int eps = 0;
        srcsize -= 2;  // fudge factor.
        lastSamp = samp = (SInt32) *src;
        while (dst != max)
        {
            src++;
            eps += dstsize;
            if ((eps << 1) >= srcsize)
            {
                *dst = samp;
                dst++;
                samp = (*src + lastSamp) >> 1;
                lastSamp = samp;
                eps -= srcsize;
            } // if
        } // while
        #else
        // Arbitrary downsampling based on Abrash's interpretation of
        //  Bresenham's run length sliced line rendering algorithm.
        // !!! FIXME: Needs better interpolation.
        register int dstsize = _dstsize;
        register int wholestep = _srcsize / dstsize;
        register int adjup = (_srcsize % dstsize);
        register int adjdown = dstsize << 1;
        register int errorterm = adjup - adjdown;
        register int initialsampcount = (wholestep >> 1);

        adjup <<= 1;
        if (wholestep & 0x01)
            errorterm += size;
        else
        {
            if (adjup == 0)
                initialsampcount--;
        } // else

        *dst = *src;
        dst++;
        src += initialsampcount;
        while (dst != max)
        {
            if ((errorterm += adjup) <= 0)
                src += wholestep;
            else
            {
                src += wholestep + 1;
                errorterm -= adjdown;
            } // else

            samp = ( (((SInt32) *src) - 128) + lastSamp ) >> 1;
            *dst = (SInt8) samp;
            dst++;
            lastSamp = samp;
        } // while
        #endif
    } // else

    return(AL_TRUE);
} // __alResampleMono8


ALboolean __alResampleMono16(ALvoid *_src, ALsizei _srcsize,
                             ALvoid *_dst, ALsizei _dstsize)
{
    register ALfloat ratio = ((ALfloat) _dstsize) / ((ALfloat) _srcsize);
    register SInt16 *dst = (SInt16 *) _dst;
    register SInt16 *max = dst + (_dstsize >> 1);
    register SInt16 *src = (SInt16 *) _src;
    register SInt32 lastSamp = (SInt32) *src;
    register SInt32 samp;

    // resample to device frequency...

    if (ratio > 1.0f)  // upsampling.
    {
        if (EPSILON_EQUAL(ratio, 2.0f)) // fast path for doubling rate.
        {
            while (dst != max)
            {
                samp = (SInt32) *src;
                src++;
                dst[0] = ((samp + lastSamp) >> 1);
                dst[1] = samp;
                   lastSamp = samp;
                dst += 2;
            } // while
        } // if

        else if (EPSILON_EQUAL(ratio, 4.0f)) // fast path for quadrupling rate.
        {
            while (dst != max)
            {
                samp = (SInt32) *src;
                src++;
                dst[0] = ((samp + 3*lastSamp) >> 2);
                dst[1] = ((samp + lastSamp) >> 1);
                dst[2] = ((3*samp + lastSamp) >> 2);
                dst[3] = samp;
                dst += 4;
                lastSamp = samp;
            } // while
        } // if

        else  // arbitrary upsampling.
        {
            #if 0  // broken!
            register SInt32 linear_counter;
            register int incr;
            register int maxincr = (int) ratio;
            while (dst != max)
            {
                samp = (SInt32) *src;
                src++;
                for (incr = 0; incr < maxincr; incr++, dst++){
                    linear_counter=(incr+1);
                    *dst = ((linear_counter*samp + (maxincr-linear_counter)*lastSamp) / maxincr);
                }
                lastSamp = samp; 
            } // while
            #elif 0  // slow!
            register float fincr = 0.0f;
            while (dst != max)
            {
                samp = src[(int) round(fincr)];
                *dst = (lastSamp + samp) >> 1;
                dst++;
                fincr += ratio;
                lastSamp = samp;
            } // while
            #elif 1
            // Arbitrary upsampling based on Bresenham's line algorithm.
            // !!! FIXME: Needs better interpolation.
            register int dstsize = _dstsize;
            register int srcsize = _srcsize;
            register int eps = 0;
            srcsize -= 4;  // fudge factor.
            samp = (SInt32) *src;
            while (dst != max)
            {
                *dst = samp;
                dst++;
                eps += srcsize;
                if ((eps << 1) >= dstsize)
                {
                    src++;
                    samp = (*src + lastSamp) >> 1;
                    lastSamp = samp;
                    eps -= dstsize;
                } // if
            } // while
            #else
            // Arbitrary upsampling based on Abrash's interpretation of
            //  Bresenham's run length sliced line rendering algorithm.
            // !!! FIXME: Needs better interpolation.
            register int dstsize = _dstsize;
            register int wholestep = dstsize / _srcsize;
            register int adjup = (dstsize % _srcsize);
            register int adjdown = size << 1;
            register int errorterm = adjup - adjdown;
            register int initialsampcount = (wholestep >> 1);
            register int finalsampcount = initialsampcount;
            register int runlength;
            register SInt16 samp16;

            max = (src + (_srcsize >> 1)) - finalsampcount;
            adjup <<= 1;
            if (wholestep & 0x01)
                errorterm += _srcsize;
            else
            {
                if (adjup == 0)
                    initialsampcount--;
            } // else

            #define DO_RESAMPLING_RUNLENGTH_MACRO_MONO16(x) \
                samp = ( ((SInt32) *src) + lastSamp ) >> 1; \
                samp16 = (SInt16) samp; \
                lastSamp = samp; \
                while (x--) { *dst = samp16; dst++; } \
                src++;

            DO_RESAMPLING_RUNLENGTH_MACRO_MONO16(initialsampcount);
            while (src != max)
            {
                runlength = wholestep;
                if ((errorterm += adjup) > 0)
                {
                    runlength++;
                    errorterm -= adjdown;
                } // if
                DO_RESAMPLING_RUNLENGTH_MACRO_MONO16(runlength);
            } // while
            DO_RESAMPLING_RUNLENGTH_MACRO_MONO16(finalsampcount);
            #endif
        } // else
    } // if

    else  // downsampling
    {
        #if 0
        register float pos = 0.0f;  // !!! FIXME: SLOW!
        while (dst != max)
        {
            samp = (SInt32) src[(size_t) round(pos)];
            pos += ratio;
            *dst = ((samp + lastSamp) >> 1);
            lastSamp = samp;
            dst++;
        } // while
        #elif 1
        // Arbitrary downsampling based on Bresenham's line algorithm.
        // !!! FIXME: Needs better interpolation.
        register int dstsize = _dstsize;
        register int srcsize = _srcsize;
        register int eps = 0;
        srcsize -= 4;  // fudge factor.
        lastSamp = samp = (SInt32) *src;
        while (dst != max)
        {
            src++;
            eps += dstsize;
            if ((eps << 1) >= srcsize)
            {
                *dst = samp;
                dst++;
                samp = (*src + lastSamp) >> 1;
                lastSamp = samp;
                eps -= srcsize;
            } // if
        } // while
        #else
        // Arbitrary downsampling based on Abrash's interpretation of
        //  Bresenham's run length sliced line rendering algorithm.
        // !!! FIXME: Needs better interpolation.
        register int dstsize = _dstsize;
        register int wholestep = _srcsize / dstsize;
        register int adjup = (_srcsize % dstsize);
        register int adjdown = dstsize << 1;
        register int errorterm = adjup - adjdown;
        register int initialsampcount = (wholestep >> 1);

        adjup <<= 1;
        if (wholestep & 0x01)
            errorterm += _srcsize;
        else
        {
            if (adjup == 0)
                initialsampcount--;
        } // else

        *dst = *src;
        dst++;
        src += initialsampcount;
        while (dst != max)
        {
            if ((errorterm += adjup) <= 0)
                src += wholestep;
            else
            {
                src += wholestep + 1;
                errorterm -= adjdown;
            } // else

            samp = ( ((SInt32) *src) + lastSamp ) >> 1;
            *dst = (SInt16) samp;
            dst++;
            lastSamp = samp;
        } // while
        #endif
    } // else

    return(AL_TRUE);
} // __alResampleMono16


ALboolean __alResampleMonoFloat32(ALvoid *_src, ALsizei _srcsize,
                                  ALvoid *_dst, ALsizei _dstsize)
{
    register ALfloat ratio = ((ALfloat) _dstsize) / ((ALfloat) _srcsize);
    register Float32 *dst = (Float32 *) _dst;
    register Float32 *max = dst + (_dstsize >> 2);
    register Float32 *src = (Float32 *) _src;
    register Float32 lastSamp = *src;
    register Float32 samp;

    // resample to device frequency...

    if (ratio > 1.0f)  // upsampling.
    {
        if (EPSILON_EQUAL(ratio, 2.0f)) // fast path for doubling rate.
        {
            while (dst != max)
            {
                samp = *src;
                src++;
                dst[0] = ((samp + lastSamp) * 0.5f);
                dst[1] = samp;
                lastSamp = samp;
                dst += 2;
            } // while
        } // if

        else if (EPSILON_EQUAL(ratio, 4.0f)) // fast path for quadrupling rate.
        {
            while (dst != max)
            {
                samp = *src;
                src++;
                dst[0] = ((samp + 3.0f * lastSamp) * 0.5f);
                dst[1] = ((samp + lastSamp) * 0.5f);
                dst[2] = ((3.0f * samp + lastSamp) * 0.25f);
                dst[3] = samp;
                dst += 4;
                lastSamp = samp;
            } // while
        } // if

        else  // arbitrary upsampling.
        {
            assert(0);  // !!! FIXME: Fill this in when you finish debugging the integer version.
        } // else
    } // if

    else  // downsampling
    {
        assert(0);  // !!! FIXME: Fill this in when you finish debugging the integer version.
    } // else

    return(AL_TRUE);
} // __alResampleMonoFloat32


ALboolean __alResampleStereoFloat32(ALvoid *_src, ALsizei _srcsize,
                                    ALvoid *_dst, ALsizei _dstsize)
{
    register ALfloat ratio = ((ALfloat) _dstsize) / ((ALfloat) _srcsize);
    register Float32 *dst = (Float32 *) _dst;
    register Float32 *max = dst + (_dstsize >> 2);
    register Float32 *src = (Float32 *) _src;
    register Float32 lastSampL = src[0];
    register Float32 lastSampR = src[1];
    register Float32 sampL;
    register Float32 sampR;

    // resample to device frequency...

    if (ratio > 1.0f)  // upsampling.
    {
        if (EPSILON_EQUAL(ratio, 2.0f)) // fast path for doubling rate.
        {
            while (dst != max)
            {
                sampL = src[0];
                sampR = src[1];
                src += 2;
                dst[0] = ((sampL + lastSampL) * 0.5f);
                dst[1] = ((sampR + lastSampR) * 0.5f);
                dst[2] = sampL;
                dst[3] = sampR;
                lastSampL = sampL;
                lastSampR = sampR;
                dst += 4;
            } // while
        } // if

        else if (EPSILON_EQUAL(ratio, 4.0f)) // fast path for quadrupling rate.
        {
            while (dst != max)   // !!! FIXME: Altivec!
            {
                sampL = (SInt32) src[0];
                sampR = (SInt32) src[1];
                src += 2;
                dst[0] = ((sampL + 3*lastSampL) * 0.25f);
                dst[1] = ((sampR + 3*lastSampR) * 0.25f);
                dst[2] = ((sampL + lastSampL) * 0.5f);
                dst[3] = ((sampR + lastSampR) * 0.5f);
                dst[4] = ((3*sampL + lastSampL) * 0.25f);
                dst[5] = ((3*sampR + lastSampR) * 0.25f);
                dst[6] = sampL;
                dst[7] = sampR;
                lastSampL = sampL;
                lastSampR = sampR;
                dst += 8;
            } // while
        } // if

        else  // arbitrary upsampling.
        {
            assert(0);  // !!! FIXME: Fill this in when you finish debugging the integer version.
        } // else
    } // if

    else  // downsampling
    {
        #if 1
        // Arbitrary downsampling based on Bresenham's line algorithm.
        // !!! FIXME: Needs better interpolation.
        // !!! FIXME: Replace with "run lengths".
        register int dstsize = _dstsize;
        register int srcsize = _srcsize;
        register int eps = 0;
        srcsize -= 16;  // fudge factor.
        lastSampL = sampL = src[0];
        lastSampR = sampR = src[1];
        while (dst != max)
        {
            src += 2;
            eps += dstsize;
            if ((eps << 1) >= srcsize)
            {
                dst[0] = sampL;
                dst[1] = sampR;
                dst += 2;
                sampL = (src[0] + lastSampL) * 0.5f;
                sampR = (src[1] + lastSampR) * 0.5f;
                lastSampL = sampL;
                lastSampR = sampR;
                eps -= srcsize;
            } // if
        } // while
        #else
        assert(0);  // !!! FIXME: Fill this in when you finish debugging the integer version.
        #endif
    } // else

    return(AL_TRUE);
} // __alResampleStereoFloat32


// Use this when no conversion is needed.
ALboolean __alResampleSimpleMemcpy(ALvoid *_src, ALsizei _srcsize,
                                   ALvoid *_dst, ALsizei _dstsize)
{
    // !!! FIXME: Use vm_copy?
    assert(_srcsize == _dstsize);
    memcpy(_dst, _src, _dstsize);
    return(AL_TRUE);
} // __alResampleSimpleMemcpy


static ALboolean __alDoBufferConvert(ALcontext *ctx, ALbuffer *buffer,
                                     ALenum format, ALvoid *data,
                                     ALsizei size, ALsizei freq)
{
    ALfloat devRate = (ALfloat) (ctx->device->streamFormat.mSampleRate);
    ALint channels = (ALint) (ctx->device->streamFormat.mChannelsPerFrame);
	ALfloat ratio = devRate / ((ALfloat) freq);
    __alResampleFunc_t resample = NULL;
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
            resample = __alResampleStereo8;
            alwaysDoConvert = AL_TRUE;  // must convert to signed.
            newAlloc = ((ALsizei) ((size >> 1) * ratio)) << 1;
            buffer->bits = 8;
            buffer->channels = 2;
            buffer->compressed = AL_FALSE;
            buffer->mixFunc = __alMixStereo8;
		    break;

		case AL_FORMAT_MONO8 :
            //printf("buffering mono8 data...\n");
            resample = __alResampleMono8;
            alwaysDoConvert = AL_TRUE;  // must convert to signed.
            newAlloc = (ALsizei) (size * ratio);
            buffer->bits = 8;
            buffer->channels = 1;
            buffer->compressed = AL_FALSE;
            if ((__alHasEnabledVectorUnit) && (channels == 2))
                buffer->mixFunc = __alMixMono8_altivec;
            else
                buffer->mixFunc = __alMixMono8;
		    break;

		case AL_FORMAT_STEREO16:
            //printf("buffering stereo16 data...\n");
            resample = __alResampleStereo16;
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
            resample = __alResampleMono16;
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
            resample = __alResampleSimpleMemcpy;
            alwaysDoConvert = AL_TRUE;  // obviously, this is specialized.
            newAlloc = size;
            buffer->bits = 32;
            buffer->channels = 2;
            buffer->compressed = AL_TRUE;
            __alBufferDataFromVorbis(buffer);
            break;
        #endif

        #if SUPPORTS_AL_EXT_FLOAT32
        case AL_FORMAT_MONO_FLOAT32:
            //printf("buffering mono-float32 data...\n");
            resample = __alResampleMonoFloat32;
            newAlloc = ((ALsizei) ((size >> 2) * ratio)) << 2;
            buffer->bits = 32;
            buffer->channels = 1;
            buffer->compressed = AL_FALSE;
            if ((__alHasEnabledVectorUnit) && (channels == 2))
                buffer->mixFunc = __alMixMonoFloat32_altivec;
            else
                buffer->mixFunc = __alMixMonoFloat32;
		    break;

        case AL_FORMAT_STEREO_FLOAT32:
            //printf("buffering stereo-float32 data...\n");
            resample = __alResampleStereoFloat32;
            newAlloc = ((ALsizei) ((size >> 3) * ratio)) << 3;
            buffer->bits = 32;
            buffer->channels = 2;
            buffer->compressed = AL_FALSE;
            if ((__alHasEnabledVectorUnit) && (channels == 2))
                buffer->mixFunc = __alMixStereoFloat32_altivec;
            else
                buffer->mixFunc = __alMixStereoFloat32;
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
        resample = __alResampleSimpleMemcpy;
    } // if

    if (newAlloc != buffer->allocatedSpace)
    {
        // +32 so we can align to 16 bytes and buffer misaligned reads.
        void *ptr = realloc(buffer->mixUnalignedPtr, newAlloc + 32);
        if (ptr == NULL)
        {
            __alSetError(AL_OUT_OF_MEMORY);
            return(AL_FALSE);
        } // if
        buffer->allocatedSpace = newAlloc;
        buffer->mixUnalignedPtr = ptr;
        buffer->mixData = ptr + (16 - (((size_t) ptr) % 16));
    } // if

    if (size == 0)
        return(AL_TRUE);  // !!! FIXME: legal no-op?

    return(resample(data, size, buffer->mixData, newAlloc));
} // __alDoBufferConvert


ALAPI ALvoid ALAPIENTRY alBufferData(ALuint buffer,ALenum format,ALvoid *data,ALsizei size,ALsizei freq)
{
    ALbuffer *buf;
    register ALcontext *ctx = __alGrabContextAndGetBuffer(buffer, &buf);
    if (ctx == NULL)
        return;

    // !!! FIXME: Make sure we're not in a source's buffer queue.

    //printf("alBufferData() bid==%d, size==%d, freq==%d\n", (int) buffer, (int) size, (int) freq);

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

