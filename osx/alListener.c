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


ALvoid __alListenerInit(ALlistener *listener)
{
	listener->Position[0] = 0.0f;
	listener->Position[1] = 0.0f;
	listener->Position[2] = 0.0f;
	listener->Velocity[0] = 0.0f;
	listener->Velocity[1] = 0.0f;
	listener->Velocity[2] = 0.0f;
	listener->Forward[0] = 0.0f;
	listener->Forward[1] = 0.0f;
	listener->Forward[2] = -1.0f;
	listener->Up[0] = 0.0f;
	listener->Up[1] = 1.0f;
	listener->Up[2] = 0.0f;
	listener->Gain = 1.0f;
	listener->Environment = 0.0f;
}

ALvoid __alListenerShutdown(ALlistener *listener)
{
    // no-op.
}


#if HAVE_PRAGMA_EXPORT
#pragma export on
#endif

ALAPI ALvoid ALAPIENTRY alListenerf (ALenum pname, ALfloat value)
{
    register ALcontext *ctx = __alGrabCurrentContext();
    if (ctx == NULL)
        return;

    switch (pname)
    {
        case AL_GAIN:
            ctx->listener.Gain = value;
            break;
        default:
            __alSetError(AL_INVALID_ENUM);
            break;
    } // switch

    __alUngrabContext(ctx);
} // alListenerf


ALAPI ALvoid ALAPIENTRY alListener3f (ALenum pname, ALfloat v1, ALfloat v2, ALfloat v3)
{
    register ALcontext *ctx = __alGrabCurrentContext();
    if (ctx == NULL)
        return;

    switch(pname)
    {
        case AL_POSITION:
            ctx->listener.Position[0]=v1;
            ctx->listener.Position[1]=v2;
            ctx->listener.Position[2]=v3;
            break;
        case AL_VELOCITY:
            ctx->listener.Velocity[0]=v1;
            ctx->listener.Velocity[1]=v2;
            ctx->listener.Velocity[2]=v3;
            break;
        default:
            __alSetError(AL_INVALID_ENUM);
            break;
    } // switch

    __alUngrabContext(ctx);
} // alListener3f


ALAPI ALvoid ALAPIENTRY alListenerfv (ALenum pname, ALfloat *values)
{
    register ALcontext *ctx = __alGrabCurrentContext();
    if (ctx == NULL)
        return;

    switch (pname)
    {
        case AL_POSITION:
            ctx->listener.Position[0]=values[0];
            ctx->listener.Position[1]=values[1];
            ctx->listener.Position[2]=values[2];
//printf("listener now at %f %f %f\n", values[0],values[1],values[2]);
            break;
        case AL_VELOCITY:
            ctx->listener.Velocity[0]=values[0];
            ctx->listener.Velocity[1]=values[1];
            ctx->listener.Velocity[2]=values[2];
            break;
        case AL_ORIENTATION:
            ctx->listener.Forward[0]=values[0];
            ctx->listener.Forward[1]=values[1];
            ctx->listener.Forward[2]=values[2];
            ctx->listener.Up[0]=values[3];
            ctx->listener.Up[1]=values[4];
            ctx->listener.Up[2]=values[5];
//printf("listener now facing %f %f %f %f %f %f\n", values[0],values[1],values[2],values[3],values[4],values[5]);
            break;
        default:
            __alSetError(AL_INVALID_ENUM);
            break;
    } // switch

    __alUngrabContext(ctx);
} // alListenerfv


ALAPI ALvoid ALAPIENTRY alListeneri(ALenum pname, ALint value)
{		
    register ALcontext *ctx = __alGrabCurrentContext();
    if (ctx == NULL)
        return;

    __alSetError(AL_INVALID_ENUM);

    __alUngrabContext(ctx);
} // alListeneri


ALAPI ALvoid ALAPIENTRY alGetListeneri( ALenum pname, ALint* value )
{
    register ALcontext *ctx = __alGrabCurrentContext();
    if (ctx == NULL)
        return;

    __alSetError(AL_INVALID_ENUM);

    __alUngrabContext(ctx);
} // alGetListerneri

ALAPI ALvoid ALAPIENTRY alGetListeneriv( ALenum pname, ALint* value )
{
    register ALcontext *ctx = __alGrabCurrentContext();
    if (ctx == NULL)
        return;

    __alSetError(AL_INVALID_ENUM);

    __alUngrabContext(ctx);
} // alGetListerneri


ALAPI ALvoid ALAPIENTRY alGetListenerf( ALenum pname, ALfloat* value )
{
    register ALcontext *ctx = __alGrabCurrentContext();
    if (ctx == NULL)
        return;

    switch (pname)
    {
            case AL_GAIN:
            *value=ctx->listener.Gain;
            break;
        default:
            __alSetError(AL_INVALID_ENUM);
            break;
    } // switch

    __alUngrabContext(ctx);
} // alGetListenerf


ALAPI ALvoid ALAPIENTRY alGetListener3f( ALenum pname, ALfloat* v1, ALfloat* v2, ALfloat* v3 )
{
    register ALcontext *ctx = __alGrabCurrentContext();
    if (ctx == NULL)
        return;

    switch (pname)
    {
        case AL_POSITION:
            *v1 = ctx->listener.Position[0];
            *v2 = ctx->listener.Position[1];
            *v3 = ctx->listener.Position[2];
            break;
        case AL_VELOCITY:
            *v1 = ctx->listener.Velocity[0];
            *v2 = ctx->listener.Velocity[1];
            *v3 = ctx->listener.Velocity[2];
            break;
        default:
            __alSetError(AL_INVALID_ENUM);
            break;
    } // switch

    __alUngrabContext(ctx);
} // alGetListener3f


ALAPI ALvoid ALAPIENTRY alGetListenerfv( ALenum pname, ALfloat* values )
{
    register ALcontext *ctx = __alGrabCurrentContext();
    if (ctx == NULL)
        return;

    switch (pname)
    {
        case AL_POSITION:
            values[0]=ctx->listener.Position[0];
            values[1]=ctx->listener.Position[1];
            values[2]=ctx->listener.Position[2];
            break;
        case AL_VELOCITY:
            values[0]=ctx->listener.Velocity[0];
            values[1]=ctx->listener.Velocity[1];
            values[2]=ctx->listener.Velocity[2];
            break;
        case AL_ORIENTATION:
            values[0]=ctx->listener.Forward[0];
            values[1]=ctx->listener.Forward[1];
            values[2]=ctx->listener.Forward[2];
            values[3]=ctx->listener.Up[0];
            values[4]=ctx->listener.Up[1];
            values[5]=ctx->listener.Up[2];
            break;
        default:
            __alSetError(AL_INVALID_ENUM);
            break;
    } // switch

    __alUngrabContext(ctx);
} // alGetListenerfv


// end of alListener.c ...

