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
#pragma export on
#endif

// what the hell, it was already in the headers...
ALAPI void ALAPIENTRY alHint( ALenum target, ALenum mode )
{
    __alSetError(AL_INVALID_ENUM);
} // alHint


ALAPI ALvoid ALAPIENTRY alEnable (ALenum capability)
{
    switch (capability)
    {
        #if SUPPORTS_AL_EXT_VECTOR_UNIT
        case AL_VECTOR_UNIT_EXT:
            if (__alDetectVectorUnit())
                __alHasEnabledVectorUnit = AL_TRUE;
            else
                __alSetError(AL_INVALID_ENUM);  // extension not exposed.
            break;
        #endif

        default:
            __alSetError(AL_INVALID_ENUM);
            break;
    } // switch
} // alEnable


ALAPI ALvoid ALAPIENTRY alDisable (ALenum capability)
{
    switch (capability)
    {
        #if SUPPORTS_AL_EXT_VECTOR_UNIT
        case AL_VECTOR_UNIT_EXT:
            if (__alDetectVectorUnit())
                __alHasEnabledVectorUnit = AL_FALSE;
            else
                __alSetError(AL_INVALID_ENUM);  // extension not exposed.
            break;
        #endif

        default:
            __alSetError(AL_INVALID_ENUM);
            break;
    } // switch
} // alDisable


ALAPI ALboolean ALAPIENTRY alIsEnabled(ALenum capability)
{
    switch (capability)
    {
        #if SUPPORTS_AL_EXT_VECTOR_UNIT
        case AL_VECTOR_UNIT_EXT:
            return(__alHasEnabledVectorUnit);
        #endif

        default:
            __alSetError(AL_INVALID_ENUM);
        	return(AL_FALSE);
    } // switch

	return(AL_FALSE);
} // alIsEnabled


ALAPI ALboolean ALAPIENTRY alGetBoolean (ALenum pname)
{
    __alSetError(AL_INVALID_ENUM);
	return AL_FALSE;
} // alGetBoolean


ALAPI ALdouble ALAPIENTRY alGetDouble (ALenum pname)
{
    __alSetError(AL_INVALID_ENUM);
	return 0.0;
} // alGetDouble


ALAPI ALfloat ALAPIENTRY alGetFloat (ALenum pname)
{
    __alSetError(AL_INVALID_ENUM);
	return 0.0f;
} // alGetFloat


ALAPI ALint ALAPIENTRY alGetInteger (ALenum pname)
{
    __alSetError(AL_INVALID_ENUM);
	return 0;
} // alGetInteger


ALAPI ALvoid ALAPIENTRY alGetBooleanv (ALenum pname, ALboolean *data)
{
    __alSetError(AL_INVALID_ENUM);
} // alGetBooleanv


ALAPI ALvoid ALAPIENTRY alGetDoublev (ALenum pname, ALdouble *data)
{
    __alSetError(AL_INVALID_ENUM);
} // alGetDoublev


ALAPI ALvoid ALAPIENTRY alGetFloatv (ALenum pname, ALfloat *data)
{
    __alSetError(AL_INVALID_ENUM);
} // alGetFloatv


ALAPI ALvoid ALAPIENTRY alGetIntegerv (ALenum pname, ALint *data)
{
    __alSetError(AL_INVALID_ENUM);
} // alGetIntegerv


ALAPI const ALubyte * ALAPIENTRY alGetString (ALenum pname)
{
    switch (pname)
    {
        case AL_VENDOR:
            return (const ALubyte *) "Ryan C. Gordon (icculus.org)";

        case AL_VERSION:
            return (const ALubyte *) "OpenAL 1.0.0";

        case AL_RENDERER:
		    return (const ALubyte *) "CoreAudio";

        case AL_EXTENSIONS:
            return __alCalculateExtensions(AL_FALSE);

        default:
            __alSetError(AL_INVALID_ENUM);
            break;
    } // switch

    return(NULL);
} // alGetString


ALAPI ALvoid ALAPIENTRY alDopplerFactor (ALfloat value)
{
    register ALcontext *ctx = __alGrabCurrentContext();
    if (ctx == NULL)
        return;

    if (value >= 0.0f)
        ctx->dopplerFactor = value;
    else
        __alSetError(AL_INVALID_VALUE);

    __alUngrabContext(ctx);
} // alDopplerFactor


ALAPI ALvoid ALAPIENTRY alDopplerVelocity (ALfloat value)
{
    register ALcontext *ctx = __alGrabCurrentContext();
    if (ctx == NULL)
        return;

    if (value > 0.0f)
        ctx->dopplerVelocity = value;
    else
        __alSetError(AL_INVALID_VALUE);

    __alUngrabContext(ctx);
} // alDopplerVelocity


ALAPI ALvoid ALAPIENTRY alPropagationSpeed (ALfloat value)
{
    register ALcontext *ctx = __alGrabCurrentContext();
    if (ctx == NULL)
        return;

    if (value > 0.0f)
        ctx->propagationSpeed = value;
    else
        __alSetError(AL_INVALID_VALUE);

    __alUngrabContext(ctx);
} // alPropagationSpeed


ALAPI ALvoid ALAPIENTRY	alDistanceModel (ALenum value)
{
    register ALcontext *ctx = __alGrabCurrentContext();
    if (ctx == NULL)
        return;

    switch (value)
    {
        case AL_NONE:
        case AL_INVERSE_DISTANCE:
        case AL_INVERSE_DISTANCE_CLAMPED:
            ctx->distanceModel = value;
            break;
        default:
            __alSetError(AL_INVALID_VALUE);
            break;
    } // switch

    __alUngrabContext(ctx);
} // alDistanceModel

#if HAVE_PRAGMA_EXPORT
#pragma export off
#endif

// end of alState.c ...

