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
static ALubyte *__alcDetermineDefaultDeviceName(ALboolean isOutput);
static ALCdevice* __alcOpenDeviceInternal(const ALubyte *deviceName,
                                          ALboolean isOutput, ALsizei freq);

#if SUPPORTS_ALC_EXT_CAPTURE
static ALvoid __alcStartCaptureIO(ALdevice *device);
static ALvoid __alcStopCaptureIO(ALdevice *device);
#endif

#if SUPPORTS_ALC_ENUMERATION_EXT
static ALubyte *__alcDetermineDeviceNameList(ALboolean isOutput);
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
            return(__alcDetermineDefaultDeviceName(AL_TRUE));

        #if SUPPORTS_ALC_ENUMERATION_EXT
		else if (param == ALC_DEVICE_SPECIFIER)
            return(__alcDetermineDeviceNameList(AL_TRUE));
        #else
		else if (param == ALC_DEVICE_SPECIFIER)
        {
            __alcSetError(NULL, ALC_INVALID_DEVICE);
            return(NULL);
        } // else if
        #endif

        #if SUPPORTS_ALC_EXT_CAPTURE
        else if (param == ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER)
            return(__alcDetermineDefaultDeviceName(AL_FALSE));

		else if (param == ALC_CAPTURE_DEVICE_SPECIFIER)
            return(__alcDetermineDeviceNameList(AL_FALSE));
        #endif

        __alcSetError((ALdevice *) device, ALC_INVALID_ENUM);
    } // if


    // Fill in device-specific extensions here...

    __alcSetError((ALdevice *) device, ALC_INVALID_ENUM);
	return (const ALubyte *) "";
} // alcGetString


ALCAPI ALvoid ALCAPIENTRY alcGetIntegerv(ALCdevice *device,ALenum param,ALsizei size,ALint *data)
{
    ALdevice *dev = (ALdevice *) device;
    switch (param)
    {
        #if SUPPORTS_ALC_EXT_SPEAKER_ATTRS
            case ALC_SPEAKER_COUNT:
                if (size < sizeof (ALint))
                    __alcSetError((ALdevice *) dev, ALC_INVALID_VALUE);
                else
                    *data = dev->speakers; // if capture device, this is 0.
                break;
        #endif

        #if SUPPORTS_ALC_EXT_CAPTURE
            case ALC_CAPTURE_SAMPLES:
                if (!dev->isInputDevice)
                    __alcSetError((ALdevice *) dev, ALC_INVALID_DEVICE);
                else if (size < sizeof (ALint))
                    __alcSetError((ALdevice *) dev, ALC_INVALID_VALUE);
                else
                {
                    ALsizei bufsize;
                    __alGrabDevice(dev);
                    bufsize = __alRingBufferSize(&dev->capture.ring);
                    __alUngrabDevice(dev);
                    *data = bufsize / dev->capture.formatSize;
                } // else
                break;
        #endif

        // !!! FIXME: fill this in.

        default:
            __alcSetError(dev, ALC_INVALID_ENUM);
    } // switch
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

    #if SUPPORTS_ALC_EXT_SPEAKER_ATTRS
    PROC_ADDRESS(alcGetSpeakerfv);
    PROC_ADDRESS(alcSpeakerf);
    PROC_ADDRESS(alcSpeakerfv);
    #endif

    #if SUPPORTS_ALC_EXT_CAPTURE
    PROC_ADDRESS(alcCaptureOpenDevice);
    PROC_ADDRESS(alcCaptureCloseDevice);
    PROC_ADDRESS(alcCaptureStart);
    PROC_ADDRESS(alcCaptureStop);
    PROC_ADDRESS(alcCaptureSamples);
    #endif

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

    #if SUPPORTS_ALC_EXT_SPEAKER_ATTRS
    ENUM_VALUE(ALC_SPEAKER_COUNT);
    ENUM_VALUE(ALC_SPEAKER_GAIN);
    ENUM_VALUE(ALC_SPEAKER_AZIMUTH);
    ENUM_VALUE(ALC_SPEAKER_ELEVATION);
    #endif

    #if SUPPORTS_ALC_EXT_CAPTURE
    ENUM_VALUE(ALC_CAPTURE_SAMPLES);
    ENUM_VALUE(ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);
    ENUM_VALUE(ALC_CAPTURE_DEVICE_SPECIFIER);
    #endif

	return ALC_NO_ERROR;
} // alcGetEnumValue



#if SUPPORTS_ALC_EXT_SPEAKER_ATTRS

static inline ALboolean __alcInvalidSpeakerId(ALdevice *device, ALuint spk)
{
    return(spk >= device->speaker);
} // __alcInvalidSpeakerId


ALCAPI ALvoid ALCAPIENTRY alcGetSpeakerfv(ALCdevice *dev, ALuint spk, ALenum param, ALfloat *v)
{
    ALdevice *device = (ALdevice *) dev;
    if (dev == NULL)
        __alcSetError(device, ALC_INVALID_DEVICE);
    else if (v == NULL)
        __alcSetError(device, ALC_INVALID_VALUE);
    else if (__alcInvalidSpeakerId(device, spk));
        __alcSetError(device, ALC_INVALID_NAME);
    else
    {
        switch (param)
        {
            case ALC_SPEAKER_GAIN:
                *v = device->speakergains[spk];
                break;

            case ALC_SPEAKER_AZIMUTH:
                *v = device->speakerazimuths[spk];
                break;

            case ALC_SPEAKER_ELEVATION:
                *v = device->speakerelevations[spk];
                break;

            default:
                __alcSetError(device, ALC_INVALID_ENUM);
        } // switch
    } // else
} // alcGetSpeakerfv


ALCAPI ALvoid ALCAPIENTRY alcSpeakerfv(ALCdevice *dev, ALuint spk, ALfloat *v)
{
    ALdevice *device = (ALdevice *) dev;
    if (dev == NULL)
        __alcSetError(device, ALC_INVALID_DEVICE);
    else if (v == NULL)
        __alcSetError(device, ALC_INVALID_VALUE);
    else if (__alcInvalidSpeakerId(device, spk));
        __alcSetError(device, ALC_INVALID_NAME);
    else
    {
        switch (param)
        {
            case ALC_SPEAKER_GAIN:
                if ((*v < 0.0f) || (*v > 1.0f))
                    __alcSetError(device, ALC_INVALID_VALUE);
                else
                    device->speakergains[spk] = *v;
                break;

            case ALC_SPEAKER_AZIMUTH:
                device->speakerazimuths[spk] = *v;
                __alcTriangulateSpeakers(device);
                break;

            case ALC_SPEAKER_ELEVATION:
                device->speakerelevations[spk] = *v;
                __alcTriangulateSpeakers(device);
                break;

            default:
                __alcSetError(device, ALC_INVALID_ENUM);
        } // switch
    } // else
} // alcSpeakerfv


ALCAPI ALvoid ALCAPIENTRY alcSpeakerf(ALCdevice *dev, ALuint spk, ALfloat x)
{
    ALfloat xyz[3] = { x, 0.0f, 0.0f };
    alcSpeakerfv(dev, spk, xyz);
} // alcSpeakerf
#endif


ALCAPI ALCcontext * ALCAPIENTRY alcProcessContext( ALCcontext *alcHandle )
{
    ALcontext *ctx = (ALcontext *) alcHandle;
    ctx->suspended = AL_FALSE;
    return(alcHandle);
} // alcProcessContext


ALCAPI ALvoid ALCAPIENTRY alcSuspendContext( ALCcontext *alcHandle )
{
    ALcontext *ctx = (ALcontext *) alcHandle;
    ctx->suspended = AL_TRUE;
} // alcSuspendContext


#if SUPPORTS_ALC_EXT_CAPTURE
ALCAPI ALCdevice* ALCAPIENTRY alcCaptureOpenDevice(const ALubyte *deviceName,
                                                    ALuint freq, ALenum fmt,
                                                    ALsizei bufsize)
{
    ALdevice *retval;
    ALuint fmtsize = 0;

    // !!! FIXME: Move to seperate function in alBuffer.c ?
    if      (fmt == AL_FORMAT_MONO8) fmtsize = 1;
    else if (fmt == AL_FORMAT_MONO16) fmtsize = 2;
    else if (fmt == AL_FORMAT_STEREO8) fmtsize = 2;
    else if (fmt == AL_FORMAT_STEREO16) fmtsize = 4;
    #if SUPPORTS_AL_EXT_FLOAT32
    else if (fmt == AL_FORMAT_MONO_FLOAT32) fmtsize = 4;
    else if (fmt == AL_FORMAT_STEREO_FLOAT32) fmtsize = 8;
    #endif
    else return(NULL);

    retval = (ALdevice *) __alcOpenDeviceInternal(deviceName, AL_FALSE, freq);
    if (!retval)
        return(NULL);

    if (!__alRingBufferInit(&retval->capture.ring, bufsize * fmtsize))
    {
        alcCloseDevice((ALCdevice *) retval);
        return(NULL);
    } // if

    retval->isInputDevice = AL_TRUE;
    retval->capture.started = AL_FALSE;
    retval->capture.formatSize = fmtsize;
    retval->capture.format = fmt;
    retval->capture.freq = freq;
    retval->capture.resampled = NULL;
    retval->capture.converted = NULL;

    fprintf(stderr, "WARNING: ALC_EXT_capture specification is subject to change!\n\n");
    return((ALCdevice *) retval);
} // alcCaptureOpenDevice


ALCAPI ALvoid ALCAPIENTRY alcCaptureCloseDevice(ALCdevice *device)
{
    ALdevice *dev = (ALdevice *) device;
    __alRingBufferShutdown(&dev->capture.ring);
    free(dev->capture.resampled);
    free(dev->capture.converted);
    dev->capture.resampled = NULL;
    dev->capture.converted = NULL;
    alcCloseDevice(device);
} // alcCaptureCloseDevice


ALCAPI ALvoid ALCAPIENTRY alcCaptureStart(ALCdevice *device)
{
    ALdevice *dev = (ALdevice *) device;
    if (!dev->isInputDevice)
        __alcSetError(dev, ALC_INVALID_DEVICE);
    else if (!dev->capture.started)
    {
        dev->capture.started = AL_TRUE;
        __alcStartCaptureIO(dev);
    } // else if
} // alcCaptureStart


ALCAPI ALvoid ALCAPIENTRY alcCaptureStop(ALCdevice *device)
{
    ALdevice *dev = (ALdevice *) device;
    if (!dev->isInputDevice)
        __alcSetError(dev, ALC_INVALID_DEVICE);
    else if (dev->capture.started)
    {
        dev->capture.started = AL_FALSE;
        __alcStopCaptureIO(dev);
    } // else if
} // alcCaptureStart


ALCAPI ALvoid ALCAPIENTRY alcCaptureSamples(ALCdevice *device, ALvoid *buf,
                                            ALsizei samps)
{
    ALdevice *dev = (ALdevice *) device;
    ALuint fmtsize = dev->capture.formatSize;
    ALsizei avail;

    if (!dev->isInputDevice)
    {
        __alcSetError(dev, ALC_INVALID_DEVICE);
        return;
    } // if

    __alGrabDevice(dev);
    avail = __alRingBufferSize(&dev->capture.ring) / fmtsize;
    if (avail < samps)
        __alcSetError(dev, AL_ILLEGAL_COMMAND);
    else
        __alRingBufferGet(&dev->capture.ring, (UInt8 *) buf, samps * fmtsize);
    __alUngrabDevice(dev);
} // alcCaptureSamples
#endif


static ALvoid __alcSetSpeakerDefaults(ALdevice *dev)
{
    UInt32 i;

    dev->speakerConfig = SPKCFG_STDSTEREO;
    if (dev->speakers > 2)
        dev->speakerConfig = SPKCFG_POSATTENUATION;

    if (dev->speakers == 0)  // may be capture device, etc...
        return;

    for (i = 0; i < dev->speakers; i++)  // fill in sane defaults...
    {
        dev->speakerazimuths[i] = 0.0f;
        dev->speakerelevations[i] = 0.0f;
        dev->speakergains[i] = 1.0f;
        i++;
    } // for

    switch (dev->speakers)
    {
        case 1:
            dev->speakerazimuths[0] = 0.0f;
            break;

        case 2:
            dev->speakerazimuths[0] = -90.0f;
            dev->speakerazimuths[1] = 90.0f;
            break;

        case 8:  // 7.1 setup
            dev->speakerazimuths[0] = -35.0f;
            dev->speakerazimuths[1] = 35.0f;
            dev->speakerazimuths[2] = -80.0f;
            dev->speakerazimuths[3] = 80.0f;
            dev->speakerazimuths[4] = -125.0f;
            dev->speakerazimuths[5] = 125.0f;
            dev->speakerazimuths[6] = -170.0f;
            dev->speakerazimuths[7] = 170.0f;
            break;

        case 4:  // Quadiphonic  !!! FIXME
        case 5:  // 4.1 setup.   !!! FIXME
        case 6:  // 5.1 setup.   !!! FIXME
        default:
            assert(0);
            break;
    } // switch

    // !!! FIXME: Override default speaker attributes with config file.

    #if USE_VBAP
    __alcTriangulateSpeakers(dev);
    #endif
} // __alcGetDefaultSpeakerPos


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

        #if FORCE_ALTIVEC
        // If this build forces Altivec usage, fail if we don't have it.
        if (__alHasEnabledVectorUnit == AL_FALSE)
            return(AL_FALSE);
        #endif

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


static ALubyte *__alcDetermineDefaultDeviceName(ALboolean isOutput)
{
    // !!! FIXME: Race condition! mutex this!
    Boolean isInput = isOutput ? FALSE : TRUE;
    Boolean outWritable;
    OSStatus error;
    UInt32 count;
    AudioDeviceID dev;
    static char *buf = NULL;  // !!! FIXME: Minor memory leak.
    AudioHardwarePropertyID defDev;

    if (buf) return(buf);  // !!! FIXME: Can the default change during the lifetime of the system?

    if (isOutput)
        defDev = kAudioHardwarePropertyDefaultOutputDevice;
    else
        defDev = kAudioHardwarePropertyDefaultInputDevice;

    count = sizeof(AudioDeviceID);
    error = AudioHardwareGetProperty(defDev, &count, &dev);
    if (error != kAudioHardwareNoError)
        return(NULL);

    error = AudioDeviceGetPropertyInfo(dev, 0, isInput, kAudioDevicePropertyDeviceName, &count, &outWritable);
    if (error != kAudioHardwareNoError)
        return(NULL);

    buf = (ALubyte *) calloc(1, count + 1);
    if (buf == NULL)
        return(NULL);

    error = AudioDeviceGetProperty(dev, 0, isInput, kAudioDevicePropertyDeviceName, &count, buf);
    if (error != kAudioHardwareNoError)
        return(NULL);

    return(buf);
} // __alcDetermineDefaultDeviceName


#if SUPPORTS_ALC_ENUMERATION_EXT
static ALubyte *__alcDetermineDeviceNameList(ALboolean isOutput)
{
    // !!! FIXME: Race condition! mutex this!
    Boolean isInput = isOutput ? FALSE : TRUE;
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

    for (i = 0; i < max; i++)
    {
        void *ptr;
        AudioDeviceID dev = devs[i];

        error = AudioDeviceGetPropertyInfo(dev, 0, isInput, kAudioDevicePropertyDeviceName, &count, &outWritable);
        if (error != kAudioHardwareNoError)
            continue;

        ptr = realloc(buf, len + count + 1);
        if (ptr == NULL)
            continue;  // oh well.

        buf = ptr;

        error = AudioDeviceGetProperty(dev, 0, isInput, kAudioDevicePropertyDeviceName, &count, buf + len);
        if (error != kAudioHardwareNoError)
            continue;

        len += count;
        buf[len] = '\0';  // double null terminate.

        // !!! FIXME:
        // For input devices, we need to enumerate all data sources, so
        //  that (say) both the microphone and line-in on a given device are
        //  exposed to the application.
        if (isInput)
        {
        } // if
    } // for

    return(buf);
} // __alcDetermineDeviceNameList
#endif


static ALboolean __alcDetermineDeviceID(const ALubyte *deviceName,
                                        ALboolean isOutput,
                                        AudioDeviceID *devID)
{
    Boolean isInput = isOutput ? FALSE : TRUE;
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
        AudioHardwarePropertyID defDev;
        if (isOutput)
            defDev = kAudioHardwarePropertyDefaultOutputDevice;
        else
            defDev = kAudioHardwarePropertyDefaultInputDevice;

    	count = sizeof (AudioDeviceID);
        error = AudioHardwareGetProperty(defDev, &count, devID);
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

        error = AudioDeviceGetPropertyInfo(dev, 0, isInput, kAudioDevicePropertyDeviceName, &count, &outWritable);
        if (error != kAudioHardwareNoError)
            continue;

        if (count != nameLen)
            continue;   // can't be this one; name is different size.

        error = AudioDeviceGetProperty(dev, 0, isInput, kAudioDevicePropertyDeviceName, &count, nameBuf);
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


static ALvoid __alcMixContext(ALcontext *ctx, UInt8 *_dst,
                                UInt32 frames, UInt32 framesize)
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
        register UInt8 *dst = _dst;
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

                srcframes = buf->mixFunc(ctx, buf, src, (Float32 *) dst, srcframes);
                if (srcframes)  // exhausted current buffer?
                {
                    // adjust for next buffer in queue's starting mix point.
                    dst += (frames - srcframes) * framesize;

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

#if SUPPORTS_ALC_EXT_CAPTURE
static OSStatus __alcCaptureDevice(AudioDeviceID  inDevice, const AudioTimeStamp*  inNow, const AudioBufferList*  inInputData, const AudioTimeStamp*  inInputTime, AudioBufferList*  outOutputData, const AudioTimeStamp* inOutputTime, void* inClientData)
{
    ALdevice *dev = (ALdevice *) inClientData;
    Float32 *resampled = NULL;
    UInt8 *converted = NULL;
    ALsizei samples = 0;
    ALsizei resampsize = 0;
    ALboolean doConvert = AL_TRUE;
    ALsizei chans = dev->streamFormat.mChannelsPerFrame;

    assert(dev->isInputDevice);
    assert(dev->speakers == 0);
    assert(dev->capture.started);

    if (!inInputData)
        return kAudioHardwareNoError;

    samples = inInputData->mBuffers[0].mDataByteSize / (sizeof(Float32)*chans);
    if ((ALint) dev->streamFormat.mSampleRate == dev->capture.freq)
    {
        resampled = inInputData->mBuffers[0].mData;
        resampsize = inInputData->mBuffers[0].mDataByteSize;
    } // if
    else
    {
    	ALfloat ratio = ((ALfloat) dev->capture.freq) / ((ALfloat) dev->streamFormat.mSampleRate);
        samples = (ALsizei) (((ALfloat) samples) * ratio);
        resampsize = samples * (sizeof (Float32) * chans);

        // !!! FIXME: Allocate this in alcCaptureOpenDevice()!
        if (dev->capture.resampled == NULL)
        {
            void *ptr = malloc(resampsize);
            if (ptr == NULL)
                return kAudioHardwareNoError;
            dev->capture.resampled = (UInt8 *) ptr;
        } // if

        resampled = (Float32 *) dev->capture.resampled;
        // !!! FIXME: Handle > 2 channel?
        if (chans == 1)
        {
            __alResampleMonoFloat32(inInputData->mBuffers[0].mData,
                                    inInputData->mBuffers[0].mDataByteSize,
                                    resampled, resampsize);
        } // if
        else
        {
            __alResampleStereoFloat32(inInputData->mBuffers[0].mData,
                                      inInputData->mBuffers[0].mDataByteSize,
                                      resampled, resampsize);
        } // else
    } // else

    #if SUPPORTS_AL_EXT_FLOAT32
    if (chans == 1)
        doConvert = (dev->capture.format != AL_FORMAT_MONO_FLOAT32);
    else if (chans == 2)
        doConvert = (dev->capture.format != AL_FORMAT_STEREO_FLOAT32);
    #endif

    converted = (UInt8 *) resampled;
    if (doConvert)
    {
        ALboolean rc = AL_FALSE;
        // !!! FIXME: Allocate this in alcCaptureOpenDevice()!
        if (dev->capture.converted == NULL)
        {
            void *ptr = malloc(samples * dev->capture.formatSize);
            if (ptr == NULL)
                return kAudioHardwareNoError;
            dev->capture.converted = (UInt8 *) ptr;
        } // if
        converted = dev->capture.converted;

        if (chans == 1)
        {
            rc = __alConvertFromMonoFloat32(resampled, converted,
                                            dev->capture.format, samples);
        } // if
        else
        {
            rc = __alConvertFromStereoFloat32(resampled, converted,
                                              dev->capture.format, samples);
        } // else

        if (!rc)
            return kAudioHardwareNoError;  // !!! FIXME
    } // if

    __alGrabDevice(dev);
    __alRingBufferPut(&dev->capture.ring, converted,
                      dev->capture.formatSize * samples);
    __alUngrabDevice(dev);

    return kAudioHardwareNoError;
} // __alcCaptureDevice

static ALvoid __alcStartCaptureIO(ALdevice *device)
{
    assert(device->isInputDevice);
    AudioDeviceStart(device->device, __alcCaptureDevice);
} // __alcStartCaptureIO

static ALvoid __alcStopCaptureIO(ALdevice *device)
{
    assert(device->isInputDevice);
    AudioDeviceStop(device->device, __alcCaptureDevice);
} // __alcStopCaptureIO
#endif


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
    ALcontext **ctxs;
    UInt8 *outDataPtr = (UInt8 *) (outOutputData->mBuffers[0].mData);
    UInt32 frames = outOutputData->mBuffers[0].mDataByteSize;
    UInt32 framesize;

    __alGrabDevice(dev);  // potentially long lock...

    framesize = (sizeof (Float32) * dev->streamFormat.mChannelsPerFrame);
    frames /= framesize;

    for (ctxs = dev->createdContexts; *ctxs != NULL; ctxs++)
    {
        ALcontext *ctx = *ctxs;

        // Technically, this would be a race condition, but it can't
        //  actually segfault, and the worst that can happen is that we
        //  swallow the cost of a mutex lock and function call to find out
        //  we don't really have to mix this context.
        if ((!ctx->suspended) && (ctx->playingSources[0] != NULL))
            __alcMixContext(ctx, outDataPtr, frames, framesize);
    } // for

    __alUngrabDevice(dev);
    return kAudioHardwareNoError;
} // __alcMixDevice


static ALCdevice* __alcOpenDeviceInternal(const ALubyte *deviceName,
                                          ALboolean isOutput, ALsizei freq)
{
    Boolean isInput = isOutput ? FALSE : TRUE;
	AudioDeviceID device;
    AudioDeviceIOProc ioproc;
	Boolean	writable;
    OSStatus error;
    UInt32 count;
    UInt32 bufsize;
    AudioStreamBasicDescription	streamFormat;
    ALdevice *retval = NULL;
    AudioStreamBasicDescription	*streamFormats;

    if (!__alcDoFirstInit())
        return(NULL);

    if (__alcDetermineDeviceID(deviceName, isOutput, &device) == AL_FALSE)
        return(NULL);

    error = AudioDeviceGetPropertyInfo(device, 0, isInput, kAudioDevicePropertyStreamFormats, &count, &writable);
    if (error != kAudioHardwareNoError) return(NULL);
    streamFormats = (AudioStreamBasicDescription *) alloca(count);
    error = AudioDeviceGetProperty(device, 0, isInput, kAudioDevicePropertyStreamFormats, &count, streamFormats);

    error = AudioDeviceGetPropertyInfo(device, 0, isInput, kAudioDevicePropertyStreamFormat, &count, &writable);
    if (error != kAudioHardwareNoError) return(NULL);
    if (count != sizeof (streamFormat)) return(NULL);
    error = AudioDeviceGetProperty(device, 0, isInput, kAudioDevicePropertyStreamFormat, &count, &streamFormat);
    if (error != kAudioHardwareNoError) return(NULL);

    if ( (freq > 0) && (((ALsizei) streamFormat.mSampleRate) != freq) )
    {
        Float64 tmpf64 = streamFormat.mSampleRate;
        streamFormat.mSampleRate = (Float64) freq;
        error = AudioDeviceSetProperty(device, NULL, 0, isInput, kAudioDevicePropertyStreamFormat, count, &streamFormat);
        if (error != kAudioHardwareNoError)
            streamFormat.mSampleRate = tmpf64;  // oh well, we tried.
    } // if

    // !!! FIXME: we don't support mono output at the moment.
    // !!! FIXME: This is a matter of writing mixers that output mono.
    if ((isOutput) && (streamFormat.mChannelsPerFrame == 1))
        return(NULL);

    // !!! FIXME: we don't support > 2 channels input at the moment.
    if ((isInput) && (streamFormat.mChannelsPerFrame > 2))
        return(NULL);

    // As of MacOS X 10.3, there is no way to get anything _but_ a Float32
    //  output buffer, as thus, we've got no other mixers written...put some
    //  sanity checks in place in case this ever changes...

    if (streamFormat.mFormatID != kAudioFormatLinearPCM)
        return(NULL);

    if (streamFormat.mBitsPerChannel != 32)
        return(NULL);

    if ((streamFormat.mFormatFlags & kLinearPCMFormatFlagIsFloat) == 0)
        return(NULL);

    // !!! FIXME: Find magic numbers by sample rate/channel...
    if (isOutput)
    {
        bufsize = (1024 * sizeof (Float32)) * streamFormat.mChannelsPerFrame;
        count = sizeof (UInt32);
        AudioDeviceSetProperty(device, NULL, 0, isInput, kAudioDevicePropertyBufferSize, count, &bufsize);
    } // if

    retval = (ALdevice *) calloc(1, sizeof (ALdevice));
    if (retval == NULL)
        return(NULL);

    __alCreateLock(&retval->deviceLock);

    memcpy(&retval->streamFormat, &streamFormat, sizeof (streamFormat));
    retval->device = device;
    if (isInput)
        retval->speakers = 0;
    else
    {
        retval->speakers = streamFormat.mChannelsPerFrame;
        if (retval->speakers > AL_MAXSPEAKERS)
            retval->speakers = AL_MAXSPEAKERS;
    } // else

    // Set up default speaker attributes.
    __alcSetSpeakerDefaults(retval);

    ioproc = ((isOutput) ? __alcMixDevice : __alcCaptureDevice);
	if (AudioDeviceAddIOProc(device, ioproc, retval) != kAudioHardwareNoError)
    {
        AudioDeviceRemoveIOProc(device, ioproc);
        __alDestroyLock(&retval->deviceLock);
        free(retval);
        return(NULL);
    } // if

    if (isOutput)
        AudioDeviceStart(device, ioproc);

	return((ALCdevice *) retval);
} // __alcOpenDeviceInternal


#if HAVE_PRAGMA_EXPORT
#pragma export on
#endif

ALCAPI ALCdevice* ALCAPIENTRY alcOpenDevice(const ALubyte *deviceName)
{
    return(__alcOpenDeviceInternal(deviceName, AL_TRUE, -1));
} // alcOpenDevice


// !!! FIXME: how do you get errors from this? device may not be valid
// !!! FIXME:   so you can't call alcGetError(), and there's no return value.
ALCAPI ALvoid ALCAPIENTRY alcCloseDevice( ALCdevice *device )
{
    ALdevice *dev = (ALdevice *) device;
    AudioDeviceIOProc ioproc;

    // Contexts associated with this device still exist?
    if (dev->createdContexts[0] != NULL)
        __alcSetError(dev, ALC_INVALID);
    else
    {
        ioproc = ((dev->isInputDevice) ? __alcCaptureDevice : __alcMixDevice);
    	AudioDeviceStop(dev->device, ioproc);
	    AudioDeviceRemoveIOProc(dev->device, ioproc);

        // !!! FIXME: I assume this means the audio callback not only cannot
        // !!! FIXME:  start a new run, but it will not be running at this point.

        __alDestroyLock(&dev->deviceLock);
        free(dev);
    } // else
} // alcCloseDevice

#endif  // MACOSX


// end of alContext.c ...

