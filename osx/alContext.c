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

// !!! FIXME: A lot of this doesn't call __alcSetError() when it should.

static volatile ALcontext *__alCurrentContext = NULL;

ALboolean __alHasEnabledVectorUnit = AL_FALSE;


// These are CoreAudio-specific, so I predeclare them here and keep the
//  actual implementations at the end of the file...
// Even though this is really meant to be a CoreAudio-based implementation
//  of OpenAL, I tried to keep the platform dependencies more or less
//  isolated where reasonable to do so.
static ALboolean __alcConfigureDevice(ALdevice *dev, const ALint *attrlist);
static ALboolean __alcDoFirstInit(ALvoid);
static ALubyte *__alcDetermineDefaultDeviceName(ALvoid);

#if SUPPORTS_ALC_ENUMERATION_EXT
static ALubyte *__alcDetermineDeviceNameList(ALvoid);
#endif


// minor pthread wrapper...
#define __alCreateLock(lockptr) pthread_mutex_init(lockptr, NULL)
#define __alDestroyLock(lockptr) pthread_mutex_destroy(lockptr)
#define __alGrabLock(lockptr) pthread_mutex_lock(lockptr)
#define __alUngrabLock(lockptr) pthread_mutex_unlock(lockptr)


static inline ALvoid __alGrabContext(ALcontext *ctx)
{
    __alGrabLock(&ctx->contextLock);
} // __alGrabContext


ALvoid __alUngrabContext(ALcontext *ctx)
{
    __alUngrabLock(&ctx->contextLock);
} // __alUngrabContext


// Use this function to find and lock the current context in a thread-safe
//  manner. This involves two mutex contentions.
// The calling thread must call __alUngrabContext() with this function's
//  return value if the result is not NULL.
// This function may return NULL if there is no current context (or no
//  contexts exist at all).
ALcontext *__alGrabCurrentContext(ALvoid)
{
    ALcontext *retval = (ALcontext *) __alCurrentContext;
    if (retval != NULL)
        __alGrabContext(retval);

    return(retval);
} // __alGrabCurrentContext


static inline ALvoid __alGrabDevice(ALdevice *dev)
{
    __alGrabLock(&dev->deviceLock);
} // __alGrabDevice


static inline ALvoid __alUngrabDevice(ALdevice *dev)
{
    __alUngrabLock(&dev->deviceLock);
} // __alUngrabDevice



// alc error management...

static ALenum __alErrorCode = AL_NO_ERROR;

// !!! FIXME: These are per-device.
static ALvoid __alcSetError( ALdevice *dev, ALenum err )
{
    if (dev->errorCode == AL_NO_ERROR)
        dev->errorCode = err;
} // __alcSetError


ALvoid __alSetError( ALenum err )
{
    if (__alErrorCode == AL_NO_ERROR)
        __alErrorCode = err;
} // __alSetError


#if HAVE_PRAGMA_EXPORT
#pragma export on
#endif

ALAPI ALenum ALAPIENTRY alcGetError( ALCdevice *device )
{
    ALdevice *dev = (ALdevice *) device;
    ALenum retval;
    retval = dev->errorCode;
	dev->errorCode = AL_NO_ERROR;
	return(retval);
} // alcGetError


ALAPI ALenum ALAPIENTRY alGetError( ALvoid )
{
    ALenum retval;
    retval = __alErrorCode;
    __alErrorCode = AL_NO_ERROR;
    return(retval);
} // alGetError


ALCAPI const ALubyte*  ALCAPIENTRY alcGetString(ALCdevice *device, ALCenum param)
{
    if (device == NULL)
    {
        if (param == ALC_EXTENSIONS)
            return(__alCalculateExtensions(AL_TRUE));

        // !!! FIXME: This isn't part of ALC_ENUMERATION_EXT, right?
        else if (param == ALC_DEFAULT_DEVICE_SPECIFIER)
            return(__alcDetermineDefaultDeviceName());

        #if SUPPORTS_ALC_ENUMERATION_EXT
		else if (param == ALC_DEVICE_SPECIFIER)
            return(__alcDetermineDeviceNameList());
        #else
		else if (param == ALC_DEVICE_SPECIFIER)
        {
            __alcSetError(NULL, ALC_INVALID_DEVICE);
            return(NULL);
        } // else if
        #endif

        __alcSetError((ALdevice *) device, ALC_INVALID_ENUM);
    } // if


    // Fill in device-specific extensions here...

    __alcSetError((ALdevice *) device, ALC_INVALID_ENUM);
	return (const ALubyte *) "";
} // alcGetString


ALCAPI ALvoid    ALCAPIENTRY alcGetIntegerv(ALCdevice *device,ALenum param,ALsizei size,ALint *data)
{
    // !!! FIXME: fill this in.
    __alcSetError((ALdevice *) device, ALC_INVALID_ENUM);
} // alcGetInteger


ALCAPI void * ALCAPIENTRY alcCreateContext(ALCdevice *device, ALint *attrlist)
{
    ALdevice *dev = (ALdevice *) device;
    ALcontext *ctx = NULL;
    ALsizei i;

    __alGrabDevice(dev);

    for (i = 0; i < AL_MAXCONTEXTS; i++)
    {
        if (dev->createdContexts[i] == NULL)
            break;
    } // for

    if (i >= AL_MAXCONTEXTS)
        __alcSetError(dev, ALC_OUT_OF_MEMORY);  // (*shrug*).
    else if (__alcConfigureDevice(dev, attrlist) == AL_TRUE)
    {
        ALsizei j;
        
        for (j = 0, ctx = dev->contexts; j < AL_MAXCONTEXTS; j++, ctx++)
        {
            if (ctx->inUse == AL_FALSE)
                break;
        } // for
        assert(j < AL_MAXCONTEXTS);

        dev->createdContexts[i] = ctx;
        memset(ctx, '\0', sizeof (ALcontext));
        __alBuffersInit(ctx->buffers, AL_MAXBUFFERS);
        __alSourcesInit(ctx->sources, AL_MAXSOURCES);
        __alListenerInit(&ctx->listener);
        __alCreateLock(&ctx->contextLock);
        ctx->dopplerFactor = 1.0f;
        ctx->dopplerVelocity = 1.0f;
        ctx->propagationSpeed = 1.0f;
        ctx->distanceModel = AL_INVERSE_DISTANCE;
        ctx->inUse = AL_TRUE;
        ctx->suspended = AL_TRUE;
        ctx->device = dev;
    } // else

    __alUngrabDevice(dev);

    return(ctx);
} // alcCreateContext


ALCAPI ALCenum ALCAPIENTRY alcDestroyContext(ALCcontext *context)
{
    ALcontext *ctx = (ALcontext *) context;
    ALcontext **ctxs;
    ALdevice *dev;
    ALsizei i;

    if (ctx == NULL)
        return(ALC_INVALID_CONTEXT);

    dev = ctx->device;

    __alGrabContext(ctx);

    // !!! FIXME: Do sources need to be stopped prior to this call?

    __alGrabDevice(ctx->device);

    ctxs = ctx->device->createdContexts;
    for (i = 0; i < AL_MAXCONTEXTS; i++, ctxs++)
    {
        if (*ctxs == ctx)
            break;
    } // for

    if (i == AL_MAXCONTEXTS)  // not created?!
    {
        assert(0);
        __alUngrabDevice(ctx->device);  // not sure what to do with this...
        __alUngrabContext(ctx);
        return(ALC_INVALID_CONTEXT);
    } // if

    ctx->inUse = AL_FALSE;
    ctx->suspended = AL_TRUE;

    // Move dead pointer out of device's createdContext array.
    memmove(&dev->createdContexts[i], &dev->createdContexts[i+1],
            sizeof (ALcontext *) * (AL_MAXCONTEXTS - i) );

    // According to the AL spec:
    //  "Applications should not attempt to destroy a current context."
    // I take that to implicitly mean, "...or behaviour is undefined."
    //  We'll be nice and do this for them.
    if (context == __alCurrentContext)
        __alCurrentContext = NULL;

    // Now that we're definitely not current, the only contention can be
    //  with the audio callback, which is currently blocked by the device
    //  lock, and will precede to ignore this context when it obtains the
    //  lock because we're not in the createdContexts array any longer.
    __alUngrabContext(ctx);
    __alDestroyLock(&ctx->contextLock);

    __alListenerShutdown(&ctx->listener);
    __alSourcesShutdown(ctx->sources, AL_MAXSOURCES);
    __alBuffersShutdown(ctx->buffers, AL_MAXBUFFERS);

    __alUngrabDevice(dev);  // start audio callback, etc in motion again.

	return ALC_NO_ERROR;
} // alcDestroyContext


ALCAPI ALCenum ALCAPIENTRY alcMakeContextCurrent(ALCcontext *context)
{
    __alCurrentContext = (ALcontext *) context;
	return AL_TRUE;
} // alcMakeContextCurrent


// !!! FIXME: Why is this (void *) and not (ALCcontext *) ?
ALCAPI void * ALCAPIENTRY alcGetCurrentContext( ALvoid )
{
    return((void *) __alCurrentContext);
} // alcGetCurrentContext


ALCAPI ALCdevice* ALCAPIENTRY alcGetContextsDevice(ALCcontext *context)
{
    ALdevice *dev = ((ALcontext *) context)->device;
    return( (ALCdevice *) dev );
} // alcGetContextsDevice


ALCAPI ALboolean ALCAPIENTRY alcIsExtensionPresent(ALCdevice *device, ALubyte *extName)
{
    if (device == NULL)
        return(__alIsExtensionPresent(extName, AL_TRUE));

    // !!! FIXME: Fill in device-specific extensions here.
    return(AL_FALSE);
} // alcIsExtensionPresent


#define PROC_ADDRESS(x) if (strcmp(#x, (const char *)funcName) == 0) { return x; }
ALCAPI ALvoid  * ALCAPIENTRY alcGetProcAddress(ALCdevice *device, ALubyte *funcName)
{
    PROC_ADDRESS(alcCreateContext);
    PROC_ADDRESS(alcMakeContextCurrent);
    PROC_ADDRESS(alcProcessContext);
    PROC_ADDRESS(alcSuspendContext);
    PROC_ADDRESS(alcDestroyContext);
    PROC_ADDRESS(alcGetError);
    PROC_ADDRESS(alcGetCurrentContext);
    PROC_ADDRESS(alcOpenDevice);
    PROC_ADDRESS(alcCloseDevice);
    PROC_ADDRESS(alcIsExtensionPresent);
    PROC_ADDRESS(alcGetProcAddress);
    PROC_ADDRESS(alcGetEnumValue);
    PROC_ADDRESS(alcGetContextsDevice);
    PROC_ADDRESS(alcGetString);
    PROC_ADDRESS(alcGetIntegerv);
	return(NULL);
} // alcGetProcAddress


#define ENUM_VALUE(x) if (strcmp(#x, (const char *)enumName) == 0) { return x; }
ALCAPI ALenum ALCAPIENTRY alcGetEnumValue(ALCdevice *device, ALubyte *enumName)
{
    ENUM_VALUE(ALC_INVALID);
    ENUM_VALUE(ALC_FREQUENCY);
    ENUM_VALUE(ALC_REFRESH);
    ENUM_VALUE(ALC_SYNC);
    ENUM_VALUE(ALC_NO_ERROR);
    ENUM_VALUE(ALC_INVALID_DEVICE);
    ENUM_VALUE(ALC_INVALID_CONTEXT);
    ENUM_VALUE(ALC_INVALID_ENUM);
    ENUM_VALUE(ALC_INVALID_VALUE);
    ENUM_VALUE(ALC_OUT_OF_MEMORY);
    ENUM_VALUE(ALC_DEFAULT_DEVICE_SPECIFIER);
    ENUM_VALUE(ALC_DEVICE_SPECIFIER);
    ENUM_VALUE(ALC_EXTENSIONS);
    ENUM_VALUE(ALC_MAJOR_VERSION);
    ENUM_VALUE(ALC_MINOR_VERSION);
    ENUM_VALUE(ALC_ATTRIBUTES_SIZE);
    ENUM_VALUE(ALC_ALL_ATTRIBUTES);
    ENUM_VALUE(ALC_FALSE);
    ENUM_VALUE(ALC_TRUE);

	return ALC_NO_ERROR;
} // alcGetEnumValue


ALCAPI ALCcontext * ALCAPIENTRY alcProcessContext( ALCcontext *alcHandle )
{
    ALcontext *ctx = (ALcontext *) alcHandle;
    ctx->suspended = AL_FALSE;
    return(alcHandle);
} // alcProcessContext


ALCAPI void ALCAPIENTRY alcSuspendContext( ALCcontext *alcHandle )
{
    ALcontext *ctx = (ALcontext *) alcHandle;
    ctx->suspended = AL_TRUE;
} // alcSuspendContext




//----------------------------------------------------------------------------
// With the exception of some defines in the headers, I've tried to keep all
//  the CoreAudio and MacOS specifics below here...

#if HAVE_PRAGMA_EXPORT
#pragma export off
#endif

#if MACOSX

ALboolean __alDetectVectorUnit(ALvoid)
{
    static ALboolean __alcAlreadyDidVectorUnitDetection = AL_FALSE;
    static ALboolean __alcVectorUnitDetected = AL_FALSE;
    if (__alcAlreadyDidVectorUnitDetection == AL_FALSE)
    {
        OSErr err;
        long cpufeature = 0;
        err = Gestalt(gestaltPowerPCProcessorFeatures, &cpufeature);
        if (err == noErr)
        {
            if ((1 << gestaltPowerPCHasVectorInstructions) & cpufeature)
                __alcVectorUnitDetected = AL_TRUE;
        } // if
    } // if

    return(__alcVectorUnitDetected);
} // __alDetectVectorUnit


static ALboolean __alcDoFirstInit(ALvoid)
{
    static ALboolean __alcAlreadyDidFirstInit = AL_FALSE;
    if (__alcAlreadyDidFirstInit == AL_FALSE)
    {
        __alHasEnabledVectorUnit = __alDetectVectorUnit();
        __alcAlreadyDidFirstInit = AL_TRUE;
        __alCalculateExtensions(AL_TRUE);
    } // if

    return(AL_TRUE);
} // __alcDoFirstInit


static ALboolean __alcConfigureDevice(ALdevice *dev, const ALint *attrlist)
{
    ALint key;
    ALint val;
    //Float64 tmpf64;
    //AudioDeviceID device = dev->device;
    //OSStatus error;

    // Deal with attrlist...
    while ((attrlist) && (*attrlist))
    {
        key = *attrlist;
        attrlist++;
        val = *attrlist;
        attrlist++;

        switch (key)
        {
            case ALC_FREQUENCY:
                #if 1  // !!! FIXME: need to know if another context expects another format before we can try to change this.
                if ((ALint) dev->streamFormat.mSampleRate != val)
                    __alcSetError(dev, ALC_INVALID_VALUE);
                #else
                // According to the AL spec:
                //  "Context creation will fail if the combination
                //   of specified attributes can not be provided."
                // My reading of that is, "even though we are going to
                //  resample all buffer data to match the device format
                //  anyhow, if you can't get the requested format, it's
                //  fatal."
                // !!! FIXME: So why isn't this done at device creation time instead?
                tmpf64 = dev->streamFormat.mSampleRate;
                dev->streamFormat.mSampleRate = (Float64) val;
                error = AudioDeviceSetProperty(device, NULL, 0, 0, kAudioDevicePropertyStreamFormat, count, &dev->streamFormat);
                if (error != kAudioHardwareNoError)
                    dev->streamFormat.mSampleRate = tmpf64;  // oh well, we tried.
                #endif
                break;

            case ALC_SYNC:
                if (val != AL_FALSE)
                {
                    __alcSetError(dev, ALC_INVALID_VALUE);
                    return(AL_FALSE);  // only does async contexts.
                } // if
                break;

            default:
                //__alcSetError(dev, ALC_INVALID_ENUM);
                //return(AL_FALSE);
                break;
        } // switch
    } // while

    return(AL_TRUE);
} // __alcConfigureDevice


static ALubyte *__alcDetermineDefaultDeviceName(ALvoid)
{
    // !!! FIXME: Race condition! mutex this!
    Boolean outWritable;
    OSStatus error;
    UInt32 count;
    AudioDeviceID dev;
    static char *buf = NULL;  // !!! FIXME: Minor memory leak.

    if (buf) return(buf);  // !!! FIXME: Can the default change during the lifetime of the system?

    count = sizeof(AudioDeviceID);
    error = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &count, &dev);
    if (error != kAudioHardwareNoError)
        return(NULL);

    error = AudioDeviceGetPropertyInfo(dev, 0, 0, kAudioDevicePropertyDeviceName, &count, &outWritable);
    if (error != kAudioHardwareNoError)
        return(NULL);

    buf = (ALubyte *) calloc(1, count + 1);
    if (buf == NULL)
        return(NULL);

    error = AudioDeviceGetProperty(dev, 0, 0, kAudioDevicePropertyDeviceName, &count, buf);
    if (error != kAudioHardwareNoError)
        return(NULL);

    return(buf);
} // __alcDetermineDefaultDeficeName


#if SUPPORTS_ALC_ENUMERATION_EXT
static ALubyte *__alcDetermineDeviceNameList(ALvoid)
{
    // !!! FIXME: Race condition! mutex this!
    Boolean outWritable;
    OSStatus error;
    UInt32 count;
    AudioDeviceID *devs;
    static char *buf = NULL;  // !!! FIXME: Minor memory leak.
    UInt32 i;
    UInt32 max;
    size_t len = 0;

    if (buf)
    {
        free(buf);
        buf = NULL;
    } // if

    error = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &count, &outWritable);
    if (error != kAudioHardwareNoError)
        return(NULL);

    devs = (AudioDeviceID *) alloca(count);
    if (devs == NULL)
        return(AL_FALSE);

    max = count / sizeof (AudioDeviceID);

    error = AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &count, devs);
    if (error != kAudioHardwareNoError)
        return(AL_FALSE);

    for (i = 0; i < count; i++)
    {
        void *ptr;
        AudioDeviceID dev = devs[i];

        error = AudioDeviceGetPropertyInfo(dev, 0, 0, kAudioDevicePropertyDeviceName, &count, &outWritable);
        if (error != kAudioHardwareNoError)
            continue;

        ptr = realloc(buf, len + count + 1);
        if (ptr == NULL)
            continue;  // oh well.

        buf = ptr;

        error = AudioDeviceGetProperty(dev, 0, 0, kAudioDevicePropertyDeviceName, &count, buf + len);
        if (error != kAudioHardwareNoError)
            continue;

        len += count;
        buf[len] = '\0';  // double null terminate.
    } // for

    return(buf);
} // __alcDetermineDeviceNameList
#endif


static ALboolean __alcDetermineDeviceID(const ALubyte *deviceName, AudioDeviceID *devID)
{
	Boolean	outWritable;
    OSStatus error;
    UInt32 count;
    AudioDeviceID *devs;
    ALubyte *nameBuf;
    UInt32 i;
    UInt32 max;
    UInt32 nameLen;

    // no preference; use system default device.
    if (deviceName == NULL)
    {
    	count = sizeof(AudioDeviceID);
        error = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &count, devID);
        return((error == kAudioHardwareNoError) ? AL_TRUE : AL_FALSE);
    } // if

    // Try to find a device name that matches request...

    nameLen = (UInt32) (strlen(deviceName) + 1);
    nameBuf = alloca((size_t) nameLen);
    if (nameBuf == NULL)
        return(AL_FALSE);

    error = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &count, &outWritable);
    if (error != kAudioHardwareNoError)
        return(AL_FALSE);

    devs = (AudioDeviceID *) alloca(count);
    if (devs == NULL)
        return(AL_FALSE);

    max = count / sizeof (AudioDeviceID);

    error = AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &count, devs);
    if (error != kAudioHardwareNoError)
        return(AL_FALSE);

    for (i = 0; i < count; i++)
    {
        AudioDeviceID dev = devs[i];

        error = AudioDeviceGetPropertyInfo(dev, 0, 0, kAudioDevicePropertyDeviceName, &count, &outWritable);
        if (error != kAudioHardwareNoError)
            continue;

        if (count != nameLen)
            continue;   // can't be this one; name is different size.

        error = AudioDeviceGetProperty(dev, 0, 0, kAudioDevicePropertyDeviceName, &count, nameBuf);
        if (error != kAudioHardwareNoError)
            continue;

        if (strcasecmp((char *) nameBuf, (char *) deviceName) == 0)
        {
            *devID = dev;
            return(AL_TRUE);  // found it!
        } // if
    } // for

    return(AL_FALSE);  // no match.
} // __alcDetermineDeviceID


static ALvoid __alcMixContext(ALcontext *ctx, Float32 *dst, UInt32 frames)
{
    register ALsource *src;
    register ALsource **playingSources;
    register ALsource **srcs;
    register ALbuffer *buf;
    register ALsizei i;
    register ALsizei firstDeadSource = -1;
    register ALsizei playCount = 0;

    __alGrabContext(ctx);

    playingSources = ctx->playingSources;
    for (i = 0, srcs = playingSources; *srcs != NULL; srcs++, i++)
    {
        register UInt32 srcframes = frames;
        register ALuint bufname;

        playCount++;
        src = *srcs;
        assert(src->inUse);

        if (src->state == AL_PLAYING)
        {
            while (srcframes)
            {
                if (src->bufferPos >= src->bufferCount)
                {
                    if (src->looping)
                    {
                        src->bufferPos = 0;
                        continue;
                    } // if

                    src->state = AL_STOPPED;
                    break;  // no buffers left in queue.
                } // if

                bufname = src->bufferQueue[src->bufferPos];
                if (bufname == 0)  // The Zero Buffer...legal no-op.
                {
                    src->bufferPos++;
                    src->bufferReadIndex = 0;
                    continue;
                } // if

                buf = &ctx->buffers[bufname - 1];

                // new buffer in queue? Prepare it if needed.
                if ((buf->prepareBuffer) && (src->bufferReadIndex == 0))
                {
                    if (!buf->prepareBuffer(src, buf))
                    {
                        src->bufferPos++;  // oh well, skip it.
                        src->bufferReadIndex = 0;
                        continue;
                    } // if
                } // if

                srcframes = buf->mixFunc(ctx, buf, src, dst, srcframes);
                if (srcframes)  // exhausted current buffer?
                {
                    // !!! FIXME: Call rewind instead if looping...
                    if (buf->processedBuffer)
                    {
                        buf->processedBuffer(src, buf);
                        src->opaque = NULL;
                    } // if

                    src->bufferPos++;
                    src->bufferReadIndex = 0;
                } // if
            } // while
        } // if

        // Can be stopped if above mixing exhausted buffer queue or
        //  alSourceStop was called since last mixing...
        if (src->state == AL_STOPPED)
        {
            if (firstDeadSource == -1)
                firstDeadSource = i;
            *srcs = NULL;
            playCount--;
        } // if
    } // for

    // Compact playingSources array to remove stopped sources, if needed...
    assert(playCount >= 0);

    if (firstDeadSource != -1) // some or all sources have stopped.
    {
        register ALsizei i = firstDeadSource;
        playingSources += i;
        for (srcs = playingSources + 1; i < playCount; srcs++)
        {
            if (*srcs != NULL)
            {
                *playingSources = *srcs;
                playingSources++;
                i++;
            } // if
        } // for
        *playingSources = NULL;
    } // else if

    __alUngrabContext(ctx);
} // __alcMixContext


static OSStatus __alcMixDevice(AudioDeviceID  inDevice, const AudioTimeStamp*  inNow, const AudioBufferList*  inInputData, const AudioTimeStamp*  inInputTime, AudioBufferList*  outOutputData, const AudioTimeStamp* inOutputTime, void* inClientData)
{    
    // !!! FIXME: As a fastpath, return immediately if hardware volume is muted.

    // !!! FIXME: Change output to new device if user changed default output in System Preferences
    // !!! FIXME:  and this callback is attached to an alcCreateDevice(NULL). Don't change if
    // !!! FIXME:  they requested a specific device, though. Need to figure out how to do that,
    // !!! FIXME:  and the full ramifications (resample all buffers? etc).

    // !!! FIXME: What happens if the user has a USB audio device like the M-Audio Sonica and
    // !!! FIXME:  unplugs it while we're using the device? What if this happens during this
    // !!! FIXME:  audio callback?

    // !!! FIXME: Is the output buffer initialized, or do I need to initialize it in fast paths?

    ALdevice *dev = (ALdevice *) inClientData;
    __alGrabDevice(dev);  // potentially long lock...

    ALcontext **ctxs;
    Float32 *outDataPtr = (outOutputData->mBuffers[0]).mData;
    UInt32 outDataSize = outOutputData->mBuffers[0].mDataByteSize;

    outDataSize /= (sizeof (Float32) * dev->streamFormat.mChannelsPerFrame);

    for (ctxs = dev->createdContexts; *ctxs != NULL; ctxs++)
    {
        ALcontext *ctx = *ctxs;

        // Technically, this would be a race condition, but it can't
        //  actually segfault, and the worst that can happen is that we
        //  swallow the cost of a mutex lock and function call to find out
        //  we don't really have to mix this context.
        if ((!ctx->suspended) && (ctx->playingSources[0] != NULL))
            __alcMixContext(ctx, outDataPtr, outDataSize);
    } // for

    __alUngrabDevice(dev);
    return kAudioHardwareNoError;
} // __alcMixDevice


#if HAVE_PRAGMA_EXPORT
#pragma export on
#endif

ALCAPI ALCdevice* ALCAPIENTRY alcOpenDevice(const ALubyte *deviceName)
{
	AudioDeviceID device;
	Boolean	writable;
    OSStatus error;
    UInt32 count;
    UInt32 bufsize;
    AudioStreamBasicDescription	streamFormat;
    ALdevice *retval = NULL;

    __alcDoFirstInit();

    if (__alcDetermineDeviceID(deviceName, &device) == AL_FALSE)
        return(NULL);

    error = AudioDeviceGetPropertyInfo(device, 0, 0, kAudioDevicePropertyStreamFormat, &count, &writable);
    if (error != kAudioHardwareNoError) return(NULL);
    if (count != sizeof (streamFormat)) return(NULL);
    error = AudioDeviceGetProperty(device, 0, 0, kAudioDevicePropertyStreamFormat, &count, &streamFormat);
    if (error != kAudioHardwareNoError) return(NULL);


#if 1
    // !!! FIXME: This is a bad assumption. Having non-stereo output complicates the hell out of things
    // !!! FIXME:  (where are speakers positioned? How can we efficiently spatialize audio between an
    // !!! FIXME:  arbitrary number of outputs? etc), so for now, try to force output to two speakers.
    // !!! FIXME:  If we can't, we'll write silence to all but the first two channels and pray for the
    // !!! FIXME:  best.
    if ((streamFormat.mChannelsPerFrame != 2) && (writable))
    {
        UInt32 tmp = streamFormat.mChannelsPerFrame;
        streamFormat.mChannelsPerFrame = 2;
        error = AudioDeviceSetProperty(device, NULL, 0, 0, kAudioDevicePropertyStreamFormat, count, &streamFormat);
        if (error != kAudioHardwareNoError)
            streamFormat.mChannelsPerFrame = tmp;  // oh well, we tried.
    } // if

    // !!! FIXME: we can't support mono output at the moment.
    // !!! FIXME: This is a matter of improving the audio callback.
    if (streamFormat.mChannelsPerFrame == 1)
        return(NULL);
#endif

    // !!! FIXME: Find magic numbers by sample rate/channel...
    bufsize = (1024 * sizeof (Float32)) * streamFormat.mChannelsPerFrame;
    count = sizeof (UInt32);
    AudioDeviceSetProperty(device, NULL, 0, 0, kAudioDevicePropertyBufferSize, count, &bufsize);

    retval = (ALdevice *) calloc(1, sizeof (ALdevice));
    if (retval == NULL)
        return(NULL);

    memcpy(&retval->streamFormat, &streamFormat, sizeof (streamFormat));
    retval->device = device;
    __alCreateLock(&retval->deviceLock);

	if ((AudioDeviceAddIOProc(device, __alcMixDevice, retval) != kAudioHardwareNoError) ||
        (AudioDeviceStart(device, __alcMixDevice) != kAudioHardwareNoError))
    {
        
	    AudioDeviceRemoveIOProc(device, __alcMixDevice);
        __alDestroyLock(&retval->deviceLock);
        free(retval);
        return(NULL);
    } // if

	return((ALCdevice *) retval);
} // alcOpenDevice


// !!! FIXME: how do you get errors from this? device may not be valid
// !!! FIXME:   so you can't call alcGetError(), and there's no return value.
ALCAPI void ALCAPIENTRY alcCloseDevice( ALCdevice *device )
{
    ALdevice *dev = (ALdevice *) device;

    // Contexts associated with this device still exist?
    if (dev->createdContexts[0] != NULL)
        __alcSetError(dev, ALC_INVALID);
    else
    {
    	AudioDeviceStop(dev->device, __alcMixDevice);
	    AudioDeviceRemoveIOProc(dev->device, __alcMixDevice);

        // !!! FIXME: I assume this means the audio callback not only cannot
        // !!! FIXME:  start a new run, but it will not be running at this point.

        __alDestroyLock(&dev->deviceLock);
        free(dev);
    } // else
} // alcCloseDevice

#endif  // MACOSX


// end of alContext.c ...

