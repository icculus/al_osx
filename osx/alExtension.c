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

// !!! FIXME: All this code sucks. Nuke it.


#if SUPPORTS_AL_EXT_VECTOR_UNIT
// AL_EXT_vector_unit:
//  Disable altivec (or SSE2, MMX, 3DNow, etc) at runtime, for debugging
//  or benchmarking purposes. Implementation may choose default state.
//  Is queried with alIsEnabled(AL_VECTOR_UNIT_EXT) and toggled with
//  alEnable/alDisable. If this extension is not exposed, implementation may
//  still be using vectorization under the hood but has no facility to
//  toggle it on the fly. Alternately, system may not contain a vector unit.
//  It is best to choose a setting and stick with it as close to context
//  creation as possible.
static ALboolean extDetectVectorUnit(ALvoid)
{
    // Don't expose extension if there's no vector unit in the first place.
    return(__alDetectVectorUnit());
} // extDetectVectorUnit
#endif

static ALboolean extDetectAlwaysTrue(ALvoid)
{
    return(AL_TRUE);
} // extDetectAlwaysTrue


typedef struct
{
    const ALubyte *extName;
    ALboolean (*detect)(ALvoid);
    ALboolean isALC;
    ALboolean available;
} __alExtensionItem;

static __alExtensionItem __alExtensionTable[] =
{
    #if SUPPORTS_AL_EXT_VECTOR_UNIT
    { "AL_EXT_vector_unit", extDetectVectorUnit, AL_FALSE, AL_FALSE },
    #endif

    #if SUPPORTS_AL_EXT_VORBIS
    { "AL_EXT_vorbis", extDetectAlwaysTrue, AL_FALSE, AL_TRUE },
    #endif

    #if SUPPORTS_ALC_ENUMERATION_EXT
    { "ALC_ENUMERATION_EXT", extDetectAlwaysTrue, AL_TRUE, AL_TRUE },
    #endif

    #if SUPPORTS_AL_EXT_BUFFER_OFFSET
    { "AL_EXT_buffer_offset", extDetectAlwaysTrue, AL_FALSE, AL_TRUE },
    #endif

    //{ "AL_hint_MOJO", extDetectAlwaysTrue, AL_FALSE, AL_TRUE },
};

const ALubyte *__alCalculateExtensions(ALboolean isALC)
{
    // !!! FIXME: This is a minor memory leak. Move this to another scope so we can free it.
    static ALubyte *__alExtensionsString = NULL;
    static ALubyte *__alcExtensionsString = NULL;
    if (__alExtensionsString == NULL)
    {
        size_t i;
        size_t len = 1;
        size_t max = sizeof (__alExtensionTable) / sizeof (__alExtensionItem);

        for (i = 0; i < max; i++)
        {
            if (!__alExtensionTable[i].isALC)
            {
                __alExtensionTable[i].available = __alExtensionTable[i].detect();
                if (__alExtensionTable[i].available)
                    len += strlen(__alExtensionTable[i].extName) + 1;
            } // if
        } // for

        __alExtensionsString = (ALubyte *) malloc(len);
        if (__alExtensionsString == NULL)
            return((const ALubyte *) "");  // better luck next time.

        __alExtensionsString[0] = '\0';

        for (i = 0; i < max; i++)
        {
            if ((__alExtensionTable[i].isALC) || (!__alExtensionTable[i].available))
                continue;
            if (__alExtensionsString[0])
                strcat(__alExtensionsString, " ");
            strcat(__alExtensionsString, __alExtensionTable[i].extName);
        } // for
    } // if

    if (__alcExtensionsString == NULL)
    {
        register size_t i;
        register size_t len = 1;
        size_t max = sizeof (__alExtensionTable) / sizeof (__alExtensionItem);

        for (i = 0; i < max; i++)
        {
            if (__alExtensionTable[i].isALC)
            {
                __alExtensionTable[i].available = __alExtensionTable[i].detect();
                if (__alExtensionTable[i].available)
                    len += strlen(__alExtensionTable[i].extName) + 1;
            } // if
        } // for

        __alcExtensionsString = (ALubyte *) malloc(len);
        if (__alcExtensionsString == NULL)
            return((const ALubyte *) "");  // better luck next time.

        __alcExtensionsString[0] = '\0';

        for (i = 0; i < max; i++)
        {
            if ((!__alExtensionTable[i].isALC) || (!__alExtensionTable[i].available))
                continue;
            if (__alcExtensionsString[0])
                strcat(__alcExtensionsString, " ");
            strcat(__alcExtensionsString, __alExtensionTable[i].extName);
        } // for
    } // if

    return(isALC ? __alcExtensionsString : __alExtensionsString);
} // __alCalculateExtensions


ALboolean __alIsExtensionPresent(const ALbyte *extName, ALboolean isALC)
{
    size_t i;
    size_t max = sizeof (__alExtensionTable) / sizeof (__alExtensionItem);

    __alCalculateExtensions(isALC);  // just in case.

    for (i = 0; i < max; i++)
    {
        if (__alExtensionTable[i].isALC == isALC)
        {
            if (strcmp(__alExtensionTable[i].extName, extName) == 0)
            {
                if (__alExtensionTable[i].available)
                    return(AL_TRUE);
            } // if
        } // if
    } // for

    return(AL_FALSE);
} // __alIsExtensionPresent


#if HAVE_PRAGMA_EXPORT
#pragma export on
#endif


ALAPI ALboolean ALAPIENTRY alIsExtensionPresent(const ALubyte *extName)
{
    return(__alIsExtensionPresent(extName, AL_FALSE));
} // alIsExtensionPresent


#define PROC_ADDRESS(x) if (strcmp(#x, (const char *)fname) == 0) { return x; }
ALAPI ALvoid * ALAPIENTRY alGetProcAddress(const ALubyte *fname)
{
    PROC_ADDRESS(alEnable);
    PROC_ADDRESS(alDisable);
    PROC_ADDRESS(alIsEnabled);
    PROC_ADDRESS(alHint);
    PROC_ADDRESS(alGetBooleanv);
    PROC_ADDRESS(alGetIntegerv);
    PROC_ADDRESS(alGetFloatv);
    PROC_ADDRESS(alGetDoublev);
    PROC_ADDRESS(alGetString);
    PROC_ADDRESS(alGetBoolean);
    PROC_ADDRESS(alGetInteger);
    PROC_ADDRESS(alGetFloat);
    PROC_ADDRESS(alGetDouble);
    PROC_ADDRESS(alGetError);
    PROC_ADDRESS(alIsExtensionPresent);
    PROC_ADDRESS(alGetProcAddress);
    PROC_ADDRESS(alGetEnumValue);
    PROC_ADDRESS(alListenerf);
    PROC_ADDRESS(alListeneri);
    PROC_ADDRESS(alListener3f);
    PROC_ADDRESS(alListenerfv);
    PROC_ADDRESS(alGetListeneri);
    PROC_ADDRESS(alGetListenerf);
    PROC_ADDRESS(alGetListeneriv);
    PROC_ADDRESS(alGetListenerfv);
    PROC_ADDRESS(alGetListener3f);
    PROC_ADDRESS(alGenSources);
    PROC_ADDRESS(alDeleteSources);
    PROC_ADDRESS(alIsSource);
    PROC_ADDRESS(alSourcei);
    PROC_ADDRESS(alSourcef);
    PROC_ADDRESS(alSource3f);
    PROC_ADDRESS(alSourcefv);
    PROC_ADDRESS(alGetSourcei);
    PROC_ADDRESS(alGetSourceiv);
    PROC_ADDRESS(alGetSourcef);
    PROC_ADDRESS(alGetSourcefv);
    PROC_ADDRESS(alGetSource3f);
    PROC_ADDRESS(alSourcePlayv);
    PROC_ADDRESS(alSourceStopv);
    PROC_ADDRESS(alSourceRewindv);
    PROC_ADDRESS(alSourcePausev);
    PROC_ADDRESS(alSourcePlay);
    PROC_ADDRESS(alSourcePause);
    PROC_ADDRESS(alSourceRewind);
    PROC_ADDRESS(alSourceStop);
    PROC_ADDRESS(alGenBuffers);
    PROC_ADDRESS(alDeleteBuffers);
    PROC_ADDRESS(alIsBuffer);
    PROC_ADDRESS(alBufferData);
    PROC_ADDRESS(alGetBufferi);
    PROC_ADDRESS(alGetBufferf);
    PROC_ADDRESS(alGetBufferiv);
    PROC_ADDRESS(alGetBufferfv);
    PROC_ADDRESS(alGenEnvironmentIASIG);
    PROC_ADDRESS(alDeleteEnvironmentIASIG);
    PROC_ADDRESS(alIsEnvironmentIASIG);
    PROC_ADDRESS(alEnvironmentiIASIG);
    PROC_ADDRESS(alEnvironmentfIASIG);
    PROC_ADDRESS(alSourceQueueBuffers);
    PROC_ADDRESS(alSourceUnqueueBuffers);
    PROC_ADDRESS(alDopplerFactor);
    PROC_ADDRESS(alDopplerVelocity);
    PROC_ADDRESS(alDistanceModel);
    PROC_ADDRESS(alutInit);
    PROC_ADDRESS(alutExit);
    PROC_ADDRESS(alutLoadWAVFile);
    PROC_ADDRESS(alutLoadWAVMemory);
    PROC_ADDRESS(alutUnloadWAV);

	return NULL;
} // alGetProcAddress


#define ENUM_VALUE(x) if (strcmp(#x, (const char *)ename) == 0) { return x; }
ALAPI ALenum ALAPIENTRY alGetEnumValue (const ALubyte *ename)
{
    ENUM_VALUE(AL_INVALID);
	ENUM_VALUE(AL_INVALID);
	ENUM_VALUE(ALC_INVALID);
	ENUM_VALUE(AL_NONE);
	ENUM_VALUE(AL_FALSE);
	ENUM_VALUE(ALC_FALSE);
	ENUM_VALUE(AL_TRUE);
	ENUM_VALUE(ALC_TRUE);
	ENUM_VALUE(AL_SOURCE_RELATIVE);
	ENUM_VALUE(AL_CONE_INNER_ANGLE);
	ENUM_VALUE(AL_CONE_OUTER_ANGLE);
	ENUM_VALUE(AL_PITCH);
	ENUM_VALUE(AL_POSITION);
	ENUM_VALUE(AL_DIRECTION);
	ENUM_VALUE(AL_VELOCITY);
	ENUM_VALUE(AL_LOOPING);
	ENUM_VALUE(AL_BUFFER);
	ENUM_VALUE(AL_GAIN);
	ENUM_VALUE(AL_MIN_GAIN);
	ENUM_VALUE(AL_MAX_GAIN);
	ENUM_VALUE(AL_ORIENTATION);
	ENUM_VALUE(AL_REFERENCE_DISTANCE);
	ENUM_VALUE(AL_ROLLOFF_FACTOR);
	ENUM_VALUE(AL_CONE_OUTER_GAIN);
	ENUM_VALUE(AL_MAX_DISTANCE);
	ENUM_VALUE(AL_SOURCE_STATE);
	ENUM_VALUE(AL_INITIAL);
	ENUM_VALUE(AL_PLAYING);
	ENUM_VALUE(AL_PAUSED);
	ENUM_VALUE(AL_STOPPED);
	ENUM_VALUE(AL_BUFFERS_QUEUED);
	ENUM_VALUE(AL_BUFFERS_PROCESSED);
	ENUM_VALUE(AL_FORMAT_MONO8);
	ENUM_VALUE(AL_FORMAT_MONO16);
	ENUM_VALUE(AL_FORMAT_STEREO8);
	ENUM_VALUE(AL_FORMAT_STEREO16);
	ENUM_VALUE(AL_FREQUENCY);
	ENUM_VALUE(AL_SIZE);
	ENUM_VALUE(AL_UNUSED);
	ENUM_VALUE(AL_PENDING);
	ENUM_VALUE(AL_PROCESSED);
	ENUM_VALUE(ALC_MAJOR_VERSION);
	ENUM_VALUE(ALC_MINOR_VERSION);
	ENUM_VALUE(ALC_ATTRIBUTES_SIZE);
	ENUM_VALUE(ALC_ALL_ATTRIBUTES);
	ENUM_VALUE(ALC_DEFAULT_DEVICE_SPECIFIER);
	ENUM_VALUE(ALC_DEVICE_SPECIFIER);
	ENUM_VALUE(ALC_EXTENSIONS);
	ENUM_VALUE(ALC_FREQUENCY);
	ENUM_VALUE(ALC_REFRESH);
	ENUM_VALUE(ALC_SYNC);
	ENUM_VALUE(AL_NO_ERROR);
	ENUM_VALUE(AL_INVALID_NAME);
	ENUM_VALUE(AL_INVALID_ENUM);
	ENUM_VALUE(AL_INVALID_VALUE);
	ENUM_VALUE(AL_INVALID_OPERATION);
	ENUM_VALUE(AL_OUT_OF_MEMORY);
	ENUM_VALUE(ALC_NO_ERROR);
	ENUM_VALUE(ALC_INVALID_DEVICE);
	ENUM_VALUE(ALC_INVALID_CONTEXT);
	ENUM_VALUE(ALC_INVALID_ENUM);
	ENUM_VALUE(ALC_INVALID_VALUE);
	ENUM_VALUE(ALC_OUT_OF_MEMORY);
	ENUM_VALUE(AL_VENDOR);
	ENUM_VALUE(AL_VERSION);
	ENUM_VALUE(AL_RENDERER);
	ENUM_VALUE(AL_EXTENSIONS);
	ENUM_VALUE(AL_DOPPLER_FACTOR);
	ENUM_VALUE(AL_DOPPLER_VELOCITY);
	ENUM_VALUE(AL_DISTANCE_MODEL);
	ENUM_VALUE(AL_INVERSE_DISTANCE);
	ENUM_VALUE(AL_INVERSE_DISTANCE_CLAMPED);

    // extensions...
    #if SUPPORTS_AL_EXT_VECTOR_UNIT
    ENUM_VALUE(AL_VECTOR_UNIT_EXT);
    #endif

    #if SUPPORTS_AL_EXT_VORBIS
    ENUM_VALUE(AL_FORMAT_VORBIS_EXT);
    #endif

    #if SUPPORTS_AL_EXT_BUFFER_OFFSET
    ENUM_VALUE(AL_BUFFER_OFFSET_EXT);
    #endif

	return(AL_NONE);
} // alGetEnumValue

// end of alExtension.c ...

