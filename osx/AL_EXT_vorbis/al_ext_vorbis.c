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

#if !SUPPORTS_AL_EXT_VORBIS
#error You should not compile this without SUPPORTS_AL_EXT_VORBIS defined.
#endif

#include "alInternal.h"
#include "vorbis/vorbisfile.h"


#if HAVE_PRAGMA_EXPORT
#pragma export off
#endif


// Callbacks for vorbisfile...

typedef struct
{
    OggVorbis_File vf;
    ALsource *src;
    ALbuffer *buf;
    vorbis_info *info;
    int previous_section;
} VorbisOpaque;


static size_t __alExtVorbisRead(void *ptr, size_t size, size_t n, void *user)
{
    register VorbisOpaque *opaque = (VorbisOpaque *) user;
    register ALbuffer *buf = opaque->buf;
    register ALsource *src = opaque->src;
    register size_t avail = buf->allocatedSpace - src->bufferReadIndex;
    register size_t cpysize = (size * n);

    if (avail < cpysize)
        cpysize = avail;

    memcpy(ptr, ((UInt8 *) buf->mixData) + src->bufferReadIndex, cpysize);
    src->bufferReadIndex += cpysize;
    return(cpysize);
} // alExtVorbisRead


static int __alExtVorbisSeek(void *user, ogg_int64_t offset, int whence)
{
    // Just consider this a non-seekable stream. We don't need more.
    return(-1);
} // alExtVorbisSeek

static int __alExtVorbisClose(void *user)
{
    // we don't free anything here, since multiple sources can be
    //  (re)using the bitstream.
    return(0);
} // alExtVorbisClose

static long __alExtVorbisTell(void *user)
{
    // Just consider this a non-seekable stream. We don't need more.
    return(-1);
} // __alExtVorbisTell

static ov_callbacks __alExtVorbisCallbacks =
{
    __alExtVorbisRead,
    __alExtVorbisSeek,
    __alExtVorbisClose,
    __alExtVorbisTell
};


// AL_EXT_vorbis implementation ...

static inline ALvoid __alMixVorbisMono(ALcontext *ctx, ALsource *src,
                                       Float32 *dst, Float32 *in, long samples,
                                       UInt32 devchannels)
{
#if 1
    assert(0);  // !!! FIXME: Updated for > 2 channels.
#else
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

/* !!! FIXME: write altivec version and precalc whether we should use it.
    if ( (__alHasEnabledVectorUnit) && (channels == 2)
         ( (!UNALIGNED_PTR(dst)) && (!UNALIGNED_PTR(in)) ) )
    {
         __alMixVorbisMono_altivec(ctx, src, dst, in, &samples);
    } // if
*/

    while (samples--)
    {
        sample = *in;
        in++;
        dst[channel0] += sample * gain0;
        dst[channel1] += sample * gain1;
        dst[channel2] += sample * gain2;
        dst += devchannels;
    } // while
#endif
} // __alMixVorbisMono


static inline ALvoid __alMixVorbisStereo_altivec(ALcontext *ctx, ALsource *src,
                                                 Float32 *_dst, Float32 *_left,
                                                 Float32 *_right, long _samples)
{
    register Float32 gainLeft;
    register Float32 gainRight;
    register long samples = _samples;
    register Float32 *dst = _dst;
    register Float32 *left = _left;
    register Float32 *right = _right;
    register vector float vLeft;
    register vector float vRight;
    register vector float vDst1;
    register vector float vDst2;
    register vector float vInterleave1;
    register vector float vInterleave2;

    register vector unsigned char permute1 = VPERMUTE4(0, 4, 1, 5);
    register vector unsigned char permute2 = vec_splat_u8(8);
    permute2 = vec_add(permute1, permute2);

	gainLeft = gainRight = src->gain * ctx->listener.Gain;
    gainLeft *= ctx->device->speakergains[0];
    gainRight *= ctx->device->speakergains[1];

    if ( (EPSILON_EQUAL(gainLeft, 1.0f)) && (EPSILON_EQUAL(gainRight, 1.0f)) )
    {
        // No attenuation, just interleave the samples and mix.
        while (samples >= 4)
        {
            vLeft = vec_ld(0, left);
            vRight = vec_ld(0, right);
            vDst1 = vec_ld(0, dst);
            vDst2 = vec_ld(16, dst);
            left += 4;
            right += 4;
            samples -= 4;
            vInterleave1 = vec_perm(vLeft, vRight, permute1);
            vInterleave2 = vec_perm(vLeft, vRight, permute2);
            vDst1 = vec_add(vInterleave1, vDst1);
            vDst2 = vec_add(vInterleave2, vDst2);
            vec_st(vDst1, 0, dst);
            vec_st(vDst2, 16, dst);
            dst += 8;
        } // while

        while (samples--)  // catch overflow.
        {
            dst[0] += *left;
            dst[1] += *right;
            dst += 2;
            left++;
            right++;
        } // while
    } // if

    else  // attenuation is needed.
    {
        register vector float vGain;
        union
        {
            vector float v;
            float f[4];
        } conv;
        conv.f[0] = conv.f[2] = gainLeft;
        conv.f[1] = conv.f[3] = gainRight;
        vGain = vec_ld(0, &conv.v);

        while (samples >= 4)
        {
            vLeft = vec_ld(0, left);
            vRight = vec_ld(0, right);
            vDst1 = vec_ld(0, dst);
            vDst2 = vec_ld(16, dst);
            left += 4;
            right += 4;
            samples -= 4;
            vInterleave1 = vec_perm(vLeft, vRight, permute1);
            vInterleave2 = vec_perm(vLeft, vRight, permute2);
            vDst1 = vec_madd(vInterleave1, vGain, vDst1);
            vDst2 = vec_madd(vInterleave2, vGain, vDst2);
            vec_st(vDst1, 0, dst);
            vec_st(vDst2, 16, dst);
            dst += 8;
        } // while

        while (samples--)  // catch overflow.
        {
            dst[0] += (*left) * gainLeft;
            dst[1] += (*right) * gainRight;
            dst += 2;
            left++;
            right++;
        } // while
    } // else
} // __alMixVorbisStereo_altivec


static inline ALvoid __alMixVorbisStereo(ALcontext *ctx, ALsource *src,
                                         Float32 *dst, Float32 *left,
                                         Float32 *right, long samples,
                                         UInt32 devchans)
{
    if ((__alHasEnabledVectorUnit) && (devchans == 2) &&
        (!((UNALIGNED_PTR(dst))||(UNALIGNED_PTR(left))||(UNALIGNED_PTR(right)))))
    {
         __alMixVorbisStereo_altivec(ctx, src, dst, left, right, samples);
    } // if
    else
    {
        register Float32 gainLeft;
        register Float32 gainRight;

    	gainLeft = gainRight = src->gain * ctx->listener.Gain;
        gainLeft *= ctx->device->speakergains[0];
        gainRight *= ctx->device->speakergains[1];

        if ( (EPSILON_EQUAL(gainLeft, 1.0f)) && (EPSILON_EQUAL(gainRight, 1.0f)) )
        {
            // No attenuation, just interleave the samples and mix.
            while (samples--)
            {
                dst[0] += *left;
                dst[1] += *right;
                dst += 2;
                left++;
                right++;
            } // while
        } // if
        else
        {
            while (samples--)
            {
                dst[0] += (*left) * gainLeft;
                dst[1] += (*right) * gainRight;
                dst += devchans;
                left++;
                right++;
            } // while
        } // else
    } // else
} // __alMixVorbisStereo


static UInt32 __alMixVorbis(struct ALcontext_struct *ctx, struct ALbuffer_struct *buf,
                     struct ALsource_struct *src, Float32 *dst, UInt32 frames)
{
    register UInt32 channels = ctx->device->streamFormat.mChannelsPerFrame;
    register VorbisOpaque *opaque = (VorbisOpaque *) src->opaque;
    register OggVorbis_File *vf = &opaque->vf;
    register vorbis_info *info = opaque->info;
    register int previous_section = opaque->previous_section;
    register int samples = frames;

    register long rc;
    float **data;
    int section;

    while (samples)
    {
        rc = ov_read_float(vf, &data, samples, &section);

        if (rc < 0)  // error, possibly recoverable.
            continue;
        else if (rc == 0)  // EOF
            return(samples);

        samples -= rc;
        if (previous_section != section)
        {
            previous_section = section;
            info = ov_info(vf, -1);
        } // if

        // !!! FIXME: Theoretically, you _can_ have a > 2 channel bitstream.
        if (info->channels == 1)
            __alMixVorbisMono(ctx, src, dst, data[0], rc, channels);
        else if (info->channels == 2)
            __alMixVorbisStereo(ctx, src, dst, data[0], data[1], rc, channels);

        dst += (rc * channels);
    } // while

    opaque->info = info;
    opaque->previous_section = previous_section;
    return(0);
} // __alMixVorbis


static ALboolean __alPrepareBufferVorbis(ALcontext *ctx,
                                         ALsource *src,
                                         ALbuffer *buf)
{
    // !!! FIXME: Note context sample rate.
    VorbisOpaque *o = calloc(1, sizeof (VorbisOpaque));
    if (o == NULL)
    {
        __alSetError(AL_OUT_OF_MEMORY);
        return(AL_FALSE);
    } // if

    o->src = src;
    o->buf = buf;

    if (ov_open_callbacks(o, &o->vf, NULL, 0, __alExtVorbisCallbacks) != 0)
    {
        free(o);
        __alSetError(AL_INVALID_OPERATION);  // oh well.
        return(AL_FALSE);
    } // if

    o->info = ov_info(&o->vf, -1);
    o->previous_section = -1;
    src->opaque = o;
    return(AL_TRUE);
} // __alPrepareBufferVorbis


static ALvoid __alProcessedBufferVorbis(ALsource *src, ALbuffer *buf)
{
    VorbisOpaque *opaque = (VorbisOpaque *) src->opaque;
    if (opaque != NULL)
    {
        ov_clear(&opaque->vf);
        free(opaque);
    } // if
} // __alProcessedBufferVorbis


static ALvoid __alRewindBufferVorbis(ALsource *src, ALbuffer *buf)
{
// !!! FIXME: You need to hook up the seek() and tell() callbacks for this
// !!! FIXME:  to work. Don't call this before then.
#if 1
    assert(0);
#else
    VorbisOpaque *opaque = (VorbisOpaque *) src->opaque;
    if (opaque != NULL)
    {
        ov_raw_seek(&opaque->vf, 0);
        opaque->info = ov_info(&o->vf, -1);
        opaque->previous_section = -1;
    } // if
#endif
} // __alRewindBufferVorbis


static ALvoid __alDeleteBufferVorbis(ALbuffer *buf)
{
    // no-op ... we don't allocate anything as buffer-instance data currently.
} // __alDeleteBufferVorbis


ALvoid __alBufferDataFromVorbis(ALbuffer *buf)
{
    buf->prepareBuffer = __alPrepareBufferVorbis;
    buf->processedBuffer = __alProcessedBufferVorbis;
    buf->rewindBuffer = __alRewindBufferVorbis;
    buf->deleteBuffer = __alDeleteBufferVorbis;
    buf->mixFunc = __alMixVorbis;
} // __alBufferDataFromVorbis

// end of al_ext_vorbis.c ...

