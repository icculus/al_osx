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


// Internal source manipulation code...

ALvoid __alSourcesInit(ALsource *srcs, ALsizei count)
{
    register ALsizei i;
    register ALsizei max = count;
    register ALsource *src = srcs;

    memset(srcs, '\0', sizeof (ALsource) * count);

    for (i = 0; i < max; i++, src++)
    {
        src->state = AL_INITIAL;
        src->needsRecalculation = AL_TRUE;
        src->pitch = 1.0f;
        src->gain = 1.0f;
        src->maxDistance = 100000000; // ***** should be MAX_FLOAT
        src->maxGain = 1.0f;
        src->rolloffFactor = 1.0f;
        src->referenceDistance = 1.0f;
    } // for
} // __alSourcesInit


ALvoid __alSourcesShutdown(ALsource *srcs, ALsizei count)
{
    // no-op, currently.
} // __alSourcesShutdown


static inline ALsource *__alFindSource_locked(ALcontext *ctx, ALuint srcid)
{
    ALsource *retval;

	if (srcid >= AL_MAXSOURCES)
    {
	    __alSetError(AL_INVALID_NAME);
        return(NULL);
    } // if

    retval = &ctx->sources[srcid];
    if ( !(retval->inUse) )
    {
	    __alSetError(AL_INVALID_NAME);
        return(NULL);
    } // if

    return(retval);
} // __alFindSource_locked


static ALcontext *__alGrabContextAndGetSource(ALuint srcid, ALsource **srcout)
{
    ALcontext *ctx = __alGrabCurrentContext();
    
    if (ctx != NULL)
    {
        *srcout = __alFindSource_locked(ctx, srcid);
        if (*srcout == NULL)
        {
            __alUngrabContext(ctx);
            return(NULL);
        } // if
    } // if

    return(ctx);
} // __alGrabContextAndGetSource



#if SUPPORTS_AL_EXT_BUFFER_OFFSET
static inline ALbuffer *__alGetPlayingBuffer(ALcontext *ctx, ALsource *src)
{
    ALuint bid;
    ALbuffer *buf;

    if (src->state != AL_PLAYING)
        return(NULL);

    if (src->bufferCount == 0)
        return(NULL);

    bid = src->bufferQueue[src->bufferPos];
    if (bid == 0)
        return(NULL);

    buf = &ctx->buffers[bid - 1];
    if (buf->compressed)
        return(NULL);

    return(buf);
} // __alGetPlayingBuffer
#endif



#if HAVE_PRAGMA_EXPORT
#pragma export on
#endif


// Source management...

ALAPI ALvoid ALAPIENTRY alGenSources(ALsizei _n, ALuint *sources)
{
	register ALsizei i = 0;
	register ALsizei iCount = 0;
    register ALsizei n = _n;

    register ALcontext *ctx = __alGrabCurrentContext();
    if (ctx == NULL)
        return;

	if (n > 0)
	{
		// check if have enough sources available
		for (i = 1; i < AL_MAXSOURCES; i++)
		{
            if (!ctx->sources[i].inUse)
            {
				iCount++;
                if (iCount >= n)
                    break;
            } // if
		} // for
	
		if (iCount >= n)
		{
			iCount = 0;
			// allocate sources where possible...
    		for (i = 1; i < AL_MAXSOURCES; i++)
	    	{
                ALsource *src = &ctx->sources[i];
                if (!src->inUse)
                {
                    __alSourcesInit(src, 1);
                    src->inUse = AL_TRUE;
                    sources[iCount] = i;
    				if (++iCount >= n)
                        break;
				} // if
			} // for

            ctx->generatedSources += n;
		} // if
        else
		{
			__alSetError(AL_INVALID_VALUE);
		} // else
	} // if

    __alUngrabContext(ctx);
} // alGenSources


ALAPI ALvoid ALAPIENTRY alDeleteSources(ALsizei _n, ALuint *sources)
{
    register ALsizei i;
    register ALsizei n = _n;
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

	// check if it's even possible to delete the number of sources requested
    if (n > ctx->generatedSources)
    {
        __alSetError(AL_INVALID_VALUE);
        __alUngrabContext(ctx);
    	return;
    } // if
    else
    {
        // !!! FIXME: Legal to delete a playing source? If so, we need to
        // !!! FIXME:  update ctx->playingSources...
    	for (i = 0; i < n; i++)
	    {
            if ((sources[i] >= AL_MAXSOURCES) || (!ctx->sources[sources[i]].inUse))
            {
    		    __alSetError(AL_INVALID_NAME);
                __alUngrabContext(ctx);
    	    	return;
            } // if
    	} // for
    } // else

    // clear source/channel information
    for (i = 0; i < n; i++)
        __alSourcesShutdown(ctx->sources + sources[i], 1);

    ctx->generatedSources -= n;

    __alUngrabContext(ctx);
} // alDeleteSources


ALAPI ALboolean ALAPIENTRY alIsSource(ALuint source)
{
    ALsource *src;
    register ALcontext *ctx = __alGrabContextAndGetSource(source, &src);

    // ctx will be null if there's no context or the source name is bogus.
    if (ctx == NULL)
        return(AL_FALSE);

    __alUngrabContext(ctx);
    return(AL_TRUE);
} // alIsSource


// Source attributes...

ALAPI ALvoid ALAPIENTRY alSourcef (ALuint source, ALenum pname, ALfloat value)
{
    ALsource *src;
	register ALcontext *ctx = __alGrabContextAndGetSource(source, &src);
    register ALboolean needsRecalc = AL_FALSE;

    if (ctx != NULL)
	{
		switch(pname) 
		{
			case AL_PITCH:
				if (value < 0.0f)
					__alSetError(AL_INVALID_VALUE);
                else
                {
                    needsRecalc = (src->pitch != value);
					src->pitch = value;
                } // else
				break;

			case AL_GAIN:
				if (value < 0.0f)
					__alSetError(AL_INVALID_VALUE);
                else
                {
                    needsRecalc = (src->gain != value);
					src->gain = value;
                } // else
				break;

			case AL_MAX_DISTANCE:
				if (value < 0.0f)
					__alSetError(AL_INVALID_VALUE);
                else
                {
                    needsRecalc = (src->maxDistance != value);
					src->maxDistance = value;
                } // else

			case AL_MIN_GAIN:
				if ((value < 0.0f) || (value > 1.0f))
					__alSetError(AL_INVALID_VALUE);
                else
                {
                    needsRecalc = (src->minGain != value);
					src->minGain = value;
                } // else
				break;

			case AL_MAX_GAIN:
				if ((value < 0.0f) || (value > 1.0f))
					__alSetError(AL_INVALID_VALUE);
                else
                {
                    needsRecalc = (src->maxGain != value);
					src->maxGain = value;
                } // else
				break;

			case AL_ROLLOFF_FACTOR:
				if (value <= 0.0f)
					__alSetError(AL_INVALID_VALUE);
                else
                {
                    if (value > 1.0f) value = 1.0f;  // !!! FIXME: don't clamp this.
                    needsRecalc = (src->rolloffFactor != value);
					src->rolloffFactor = value;
                } // else
				break;

			case AL_REFERENCE_DISTANCE:
				if (value <= 0.0f)
					__alSetError(AL_INVALID_VALUE);
                else
                {
                    needsRecalc = (src->referenceDistance != value);
					src->referenceDistance = value;
                } // else
				break;

			default:
				__alSetError(AL_INVALID_OPERATION);
				break;
		} // switch

        if (needsRecalc)
            src->needsRecalculation = AL_TRUE;

        __alUngrabContext(ctx);
	} // if
} // alSourcef


ALAPI ALvoid ALAPIENTRY alSourcefv (ALuint source, ALenum pname, ALfloat *values)
{
    ALsource *src;
    register ALboolean needsRecalc = AL_FALSE;
	register ALcontext *ctx = __alGrabContextAndGetSource(source, &src);
    if (ctx != NULL)
	{
    	switch(pname) 
	    {
		    case AL_POSITION:
                needsRecalc = ((src->Position[0] != values[0]) ||
                               (src->Position[1] != values[1]) ||
                               (src->Position[2] != values[2]));
                if (needsRecalc)
                {
    	    		src->Position[0]=values[0];
	    	    	src->Position[1]=values[1];
		    	    src->Position[2]=values[2];
                } // if
		    	break;

    		case AL_VELOCITY:
                needsRecalc = ((src->Velocity[0] != values[0]) ||
                               (src->Velocity[1] != values[1]) ||
                               (src->Velocity[2] != values[2]));
                if (needsRecalc)
                {
    	    		src->Velocity[0]=values[0];
	    	    	src->Velocity[1]=values[1];
		    	    src->Velocity[2]=values[2];
                } // if
    			break;

	    	default:
    			__alSetError(AL_INVALID_ENUM);
    			break;
	    } // switch

        if (needsRecalc)
            src->needsRecalculation = AL_TRUE;

        __alUngrabContext(ctx);
    } // if
} // alSourcefv


ALAPI ALvoid ALAPIENTRY alSource3f (ALuint source, ALenum pname, ALfloat v1, ALfloat v2, ALfloat v3)
{
    // for now, this is good enough.
    if ((pname == AL_POSITION) || (pname == AL_VELOCITY))
    {
        ALfloat vector[3] = {v1, v2, v3};
        alSourcefv(source, pname, vector);
        return;
    } // if

    __alSetError(AL_INVALID_ENUM);
} // alSource3f


ALAPI ALvoid ALAPIENTRY alSourcei (ALuint source, ALenum pname, ALint value)
{
    ALsource *src;
    ALbuffer *buf;
    ALuint tmp;
    register ALboolean needsRecalc = AL_FALSE;
	register ALcontext *ctx = __alGrabContextAndGetSource(source, &src);

    if (ctx != NULL)
	{
	    switch(pname) 
		{
			case AL_LOOPING:
				src->looping = value;
				break;

            // AL_BUFFER nukes the buffer queue and replaces it with a one-entry queue.
			case AL_BUFFER:
				if ((src->state != AL_STOPPED) && (src->state != AL_INITIAL))
					__alSetError(AL_INVALID_OPERATION);
                else
				{
                    if (value == 0)
                        src->bufferCount = src->bufferPos = 0;
                    else
                    {
                        // !!! FIXME: This is yucky. Use logic in alBuffer.c...
                        ALboolean valid = AL_FALSE;
                        valid = ( ((value - 1) < AL_MAXBUFFERS) &&
                                      (ctx->buffers[value - 1].inUse) );

                        if (valid)
	    				{
                            src->bufferCount = 1;
                            src->bufferPos = 0;
					    	src->bufferQueue[0] = value;
				    	} // if
                        else
		    			{
	    					__alSetError(AL_INVALID_VALUE);
    					} // else
                    } // if
				} // else
				break;

            case AL_SOURCE_RELATIVE:
                if ((value != AL_FALSE) && (value != AL_TRUE))
                    __alSetError(AL_INVALID_VALUE);
                else
                {
                    needsRecalc = (src->srcRelative != value);
                    src->srcRelative = value;
                } // else
                break;

            #if SUPPORTS_AL_EXT_BUFFER_OFFSET
            case AL_BUFFER_OFFSET_EXT:
                buf = __alGetPlayingBuffer(ctx, src);
                if (buf == NULL)
                    __alSetError(AL_ILLEGAL_COMMAND);
                else
                {
                    if ((value < 0) || (value >= buf->allocatedSpace))
                        __alSetError(AL_INVALID_VALUE);
                    else
                    {
                        tmp = buf->channels * (buf->bits / 8);
                        src->bufferReadIndex = value / tmp;
                    } // else
                } // else
                break;
            #endif

			default:
				__alSetError(AL_INVALID_ENUM);
				break;
		} // switch

        if (needsRecalc)
            src->needsRecalculation = AL_TRUE;

        __alUngrabContext(ctx);
	} // if
} // alSourcei


ALAPI ALvoid ALAPIENTRY alGetSourcef (ALuint source, ALenum pname, ALfloat *value)
{
    ALsource *src;
	register ALcontext *ctx = __alGrabContextAndGetSource(source, &src);
    if (ctx != NULL)
	{
		switch(pname) 
		{
			case AL_PITCH:
				*value = src->pitch;
				break;
			case AL_GAIN:
				*value = src->gain;
				break;
			case AL_MAX_DISTANCE:
				*value = src->maxDistance;
				break;
			case AL_MIN_GAIN:
				*value = src->minGain;
				break;
			case AL_MAX_GAIN:
				*value = src->maxGain;
				break;
			case AL_ROLLOFF_FACTOR:
				*value = src->rolloffFactor;
				break;
			case AL_REFERENCE_DISTANCE:
				*value = src->referenceDistance;
				break;
			default:
				__alSetError(AL_INVALID_ENUM);
				break;
		} // switch
        __alUngrabContext(ctx);
	} // if
} // alGetSourcef


ALAPI ALvoid ALAPIENTRY alGetSource3f (ALuint source, ALenum pname, ALfloat *v1, ALfloat *v2, ALfloat *v3)
{
    ALsource *src;
	register ALcontext *ctx = __alGrabContextAndGetSource(source, &src);
    if (ctx != NULL)
	{
        switch (pname)
        {
    		case AL_POSITION:
	    		*v1 = src->Position[0];
		    	*v2 = src->Position[1];
			    *v3 = src->Position[2];
    			break;
	    	case AL_DIRECTION:
		        __alSetError(AL_INVALID_ENUM); // cone functions not implemented yet
		        break;
    		case AL_VELOCITY:
	    		*v1 = src->Velocity[0];
		    	*v2 = src->Velocity[1];
			    *v3 = src->Velocity[2];
    			break;
	    	default:
		    	__alSetError(AL_INVALID_ENUM);
			    break;
        } // switch
        __alUngrabContext(ctx);
	} // if
} // alGetSource3f


ALAPI ALvoid ALAPIENTRY alGetSourcefv (ALuint source, ALenum pname, ALfloat *values)
{
    ALsource *src;
	register ALcontext *ctx = __alGrabContextAndGetSource(source, &src);
    if (ctx != NULL)
	{
		switch(pname)
		{
			case AL_POSITION:
				values[0] = src->Position[0];
				values[1] = src->Position[1];
				values[2] = src->Position[2];
				break;
			case AL_VELOCITY:
				values[0] = src->Velocity[0];
				values[1] = src->Velocity[1];
				values[2] = src->Velocity[2];
				break;
			case AL_DIRECTION:
				__alSetError(AL_INVALID_ENUM);
				break;
			default:
				__alSetError(AL_INVALID_ENUM);
				break;
		} // switch
        __alUngrabContext(ctx);
	} // if
} // alGetSourcefv


ALAPI ALvoid ALAPIENTRY alGetSourcei (ALuint source, ALenum pname, ALint *value)
{
    ALsource *src;
    ALbuffer *buf;
    ALuint tmp;
	register ALcontext *ctx = __alGrabContextAndGetSource(source, &src);

    if (ctx != NULL)
	{
		switch(pname)
		{
			case AL_CONE_INNER_ANGLE:
			case AL_CONE_OUTER_ANGLE:
				__alSetError(AL_INVALID_ENUM); // not implemented yet
				break;
			case AL_LOOPING:
				*value=src->looping;
				break;
			case AL_BUFFER:
                // !!! FIXME: Is this the head of the queue or the first unprocessed buffer?
				*value=src->bufferQueue[0];
				break;
            case AL_SOURCE_RELATIVE:
                *value=src->srcRelative;
				break;
			case AL_SOURCE_STATE:
			    *value=src->state;
			    break;
			case AL_BUFFERS_QUEUED:
                // According to the spec:
                // "This will return 0 if the current and only bufferName is 0."
                if ((src->bufferCount == 1) && (src->bufferQueue[0] == 0))
                    *value = 0;
                else
                    *value=src->bufferCount;
                break;
			case AL_BUFFERS_PROCESSED:
                *value=src->bufferPos;
                break;

            #if SUPPORTS_AL_EXT_BUFFER_OFFSET
            case AL_BUFFER_OFFSET_EXT:
                buf = __alGetPlayingBuffer(ctx, src);
                if (buf == NULL)
                {
                    __alSetError(AL_ILLEGAL_COMMAND);
                    *value = -1;
                } // if
                else
                {
                    tmp = buf->channels * (buf->bits / 8);
                    *value = src->bufferReadIndex * tmp;
                } // else
                break;
            #endif

			default:
				__alSetError(AL_INVALID_ENUM);
				break;
		} // switch
        __alUngrabContext(ctx);
	} // if
} // alGetSourcei


ALAPI ALvoid ALAPIENTRY alGetSourceiv(ALuint source, ALenum pname, ALint *value)
{
    alGetSourcei(source, pname, value);
} // alGetSourceiv


// Source play ...

static inline ALvoid __alSourcePlay_locked(ALcontext *ctx, ALsource *src)
{
    if (src != NULL)
    {
        if (src->state != AL_PAUSED)
        {
            ALsource **srcs;

            // !!! FIXME: Is this right? Restart the current buffer, or
            // !!! FIXME:  restart the queue?
            src->bufferReadIndex = 0;

            // Make sure we're in the playing list.
            //  We might already be there if we're INITIAL from a rewind
            //  or STOPPED and the audio callback hasn't run to clean the
            //  reference out of the array yet.
            srcs = ctx->playingSources;
            while (1)  // (srcs) should always be null-terminated.
            {
                if (*srcs == src)
                    break;

                else if (*srcs == NULL)
                {
                    srcs[0] = src;
                    srcs[1] = NULL;
                    break;
                } // else if

                srcs++;
            } // while
        } // if

        src->state = AL_PLAYING;
    } // if
} // __alSourcePlay_locked

ALAPI ALvoid ALAPIENTRY alSourcePlayv(ALsizei n, ALuint *_id)
{
    if (n > 0)
	{
	    register ALcontext *ctx = __alGrabCurrentContext();
        if (ctx != NULL)
        {
		    while (n--)
    		{
                __alSourcePlay_locked(ctx, __alFindSource_locked(ctx, *_id));
                _id++;
		    } // while
            __alUngrabContext(ctx);
    	} // if
    } // if
} // alSourcePlayv

ALAPI ALvoid ALAPIENTRY alSourcePlay(ALuint source)
{
    alSourcePlayv(1, &source);
} // alSourcePlay



// Source pause...

static inline ALvoid __alSourcePause_locked(ALcontext *ctx, ALsource *src)
{
    if (src != NULL)
    {
        if (src->state == AL_PLAYING)  // !!! FIXME: An error if not playing?
            src->state = AL_PAUSED;
    } // if
} // __alSourcePause_locked


ALAPI ALvoid ALAPIENTRY alSourcePausev(ALsizei n, ALuint *_id)
{
    if (n > 0)
	{
	    register ALcontext *ctx = __alGrabCurrentContext();
        if (ctx != NULL)
        {
		    while (n--)
    		{
                __alSourcePause_locked(ctx, __alFindSource_locked(ctx, *_id));
                _id++;
		    } // while
            __alUngrabContext(ctx);
    	} // if
    } // if
} // alSourcePausev


ALAPI ALvoid ALAPIENTRY alSourcePause (ALuint source)
{
    alSourcePausev(1, &source);
} // alSourcePause



// Source stop...

static inline ALvoid __alSourceStop_locked(ALcontext *ctx, ALsource *src)
{
    // According to the spec:
    //  "Stop() applied to an INITIAL Source is a legal NOP."
    //  "Stop() applied to a STOPPED Source is a legal NOP."
    if ((!src) || (src->state == AL_INITIAL) || (src->state == AL_STOPPED))
        return;

    // __alcMixContext() can now remove it from playingSources array.
    src->state = AL_STOPPED;
    
    // According to the spec:
    //  "Removal of a given queue entry is not possible unless either the
    //  Source is STOPPED (in which case then entire queue is considered
    //  processed)..."
    // !!! FIXME: compressed buffers need to be alerted that they are processed.
    src->bufferPos = src->bufferCount;  // set whole queue to "processed".
} // __alSourceStop_locked

ALAPI ALvoid ALAPIENTRY alSourceStopv(ALsizei n, ALuint *_id)
{
    if (n > 0)
	{
	    register ALcontext *ctx = __alGrabCurrentContext();
        if (ctx != NULL)
        {
		    while (n--)
    		{
                __alSourceStop_locked(ctx, __alFindSource_locked(ctx, *_id));
                _id++;
		    } // while
            __alUngrabContext(ctx);
    	} // if
    } // if
} // alSourceStopv

ALAPI ALvoid ALAPIENTRY alSourceStop(ALuint source)
{
    alSourceStopv(1, &source);
} // alSourceStop


// Source rewind...

static inline ALvoid __alSourceRewind_locked(ALcontext *ctx, ALsource *src)
{
    if (src != NULL)
    {
        __alSourceStop_locked(ctx, src); // process buffers, etc.
        src->state = AL_INITIAL;
    } // if
} // __alSourceRewind_locked

ALAPI ALvoid ALAPIENTRY alSourceRewindv(ALsizei n, ALuint *_id)
{
    if (n > 0)
	{
	    register ALcontext *ctx = __alGrabCurrentContext();
        if (ctx != NULL)
        {
		    while (n--)
    		{
                __alSourceRewind_locked(ctx, __alFindSource_locked(ctx, *_id));
                _id++;
		    } // while
            __alUngrabContext(ctx);
    	} // if
    } // if
} // alSourceRewindv

ALAPI ALvoid ALAPIENTRY alSourceRewind(ALuint source)
{
    alSourceRewindv(1, &source);
} // alSourceRewind


// Buffer queueing...

ALAPI ALvoid ALAPIENTRY alSourceQueueBuffers (ALuint source, ALsizei n, ALuint *buffers)
{
    ALsource *src;
	register ALcontext *ctx;

    if (n < 0)
    {
        __alSetError(AL_INVALID_VALUE);
        return;
    } // if

    else if (n == 0)  // legal no-op.
    {
        return;
    } // else if

    ctx = __alGrabContextAndGetSource(source, &src);
    //{ int i; printf("Queueing buffers:"); for (i = 0; i < n; i++) { printf(" %d", buffers[i]); } printf("\n"); }
    if (ctx != NULL)
    {
        if ((src->bufferCount + n) > AL_MAXBUFFERQUEUE)
            __alSetError(AL_OUT_OF_MEMORY);  // !!! FIXME
        else
        {
            memcpy(src->bufferQueue + src->bufferCount, buffers, n * sizeof (ALuint));
            src->bufferCount += n;
        } // else
        __alUngrabContext(ctx);
    } // if
} // alSourceQueueBuffers


ALAPI ALvoid ALAPIENTRY alSourceUnqueueBuffers (ALuint source, ALsizei n, ALuint *buffers)
{
    ALsource *src;
	register ALcontext *ctx;

    if (n < 0)
    {
        __alSetError(AL_INVALID_VALUE);
        return;
    } // if

    else if (n == 0)  // legal no-op.
    {
        return;
    } // else if

    ctx = __alGrabContextAndGetSource(source, &src);
    if (ctx != NULL)
    {
        // From the spec:
        //  "The operation will fail if more buffers are requested than
        //   available [processed], leaving the destination arguments
        //   unchanged. An INVALID_VALUE error will be thrown."
        if ( ((ALuint) n) > src->bufferPos)
            __alSetError(AL_INVALID_VALUE);
        else
        {
            src->bufferCount -= n;
            src->bufferPos -= n;
            memcpy(buffers, src->bufferQueue, n * sizeof (ALuint));
            //{ int i; printf("Unqueueing buffers:"); for (i = 0; i < n; i++) { printf(" %d", buffers[i]); } printf("\n"); }
            memmove(src->bufferQueue, src->bufferQueue + n, src->bufferCount * sizeof (ALuint));
        } // else
        __alUngrabContext(ctx);
    } // if
} // alSourceUnqueueBuffers

// end of alSource.c ...

