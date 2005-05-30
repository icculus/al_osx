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
 
#ifndef _INCL_ALINTERNAL_H_
#define _INCL_ALINTERNAL_H_

// The Holy Trinity of Standard Headers.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Other Unix things...
#include <unistd.h>
#include <pthread.h>

// Public OpenAL types...
#define ALAPI
#define ALAPIENTRY
#define AL_CALLBACK
#define ALUTAPI
#define ALUTAPIENTRY
#define ALUT_CALLBACK
#include "al.h"
#include "alc.h"
#include "alut.h"

// CoreAudio and other Apple headers...
#if MACOSX
#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudio.h>
#endif

#if HAVE_PRAGMA_EXPORT
#pragma export off
#endif

// Some predeclarations...

#include "alMath.h"

#define MINVOL 0.005f
#define FULL_VOLUME 0.512f

struct ALdevice_struct;
struct ALcontext_struct;
struct ALbuffer_struct;
struct ALsource_struct;

typedef pthread_mutex_t ALlock;

// These numbers might need adjustment up or down.
#define AL_MAXBUFFERS      1024 * 2
#define AL_MAXBUFFERQUEUE  128
#define AL_MAXSOURCES      1024 * 5
#define AL_MAXCONTEXTS     4
#define AL_MAXSPEAKERS     8

// zero if altivec-aligned.
#define UNALIGNED_PTR(x) (((size_t) x) & 0x0000000F)
#define VPERMUTE4(a,b,c,d) (vector unsigned char) \
                               ( (a*4)+0, (a*4)+1, (a*4)+2, (a*4)+3, \
                                 (b*4)+0, (b*4)+1, (b*4)+2, (b*4)+3, \
                                 (c*4)+0, (c*4)+1, (c*4)+2, (c*4)+3, \
                                 (d*4)+0, (d*4)+1, (d*4)+2, (d*4)+3 )

// Ring buffers...
typedef struct
{
    UInt8 *buffer;
    ALsizei size;
    ALsizei write;
    ALsizei read;
    ALsizei used;
} __ALRingBuffer;

ALboolean __alRingBufferInit(__ALRingBuffer *ring, ALsizei size);
ALvoid __alRingBufferShutdown(__ALRingBuffer *ring);
ALsizei __alRingBufferSize(__ALRingBuffer *ring);
ALvoid __alRingBufferPut(__ALRingBuffer *ring, UInt8 *data, ALsizei size);
ALsizei __alRingBufferGet(__ALRingBuffer *ring, UInt8 *data, ALsizei size);


// Mixers...

// __alMixFunc:
//
// Called with the context, source, and specific buffer in the source's queue
//  that is to be mixed. Function is responsible for updating internal
//  source state to represent mixing progress and recalculating source
//  spatialization as appropriate (as an optimization, since stereo buffers
//  can opt to skip that processing). Decompression of the buffer as needed
//  should be done here. Function should mix (frames) sample frames to
//  buffer at (dst), which may contain audio data from other sources that
//  were mixed earlier in the pipeline. If the buffer is completely mixed
//  in this call, function should return the number of sample frames that
//  were not mixed. The caller will deal with flagging buffers as processed
//  and recalling with the next buffer in the queue (adjusting (dst) and
//  (frames) appropriately).
//
// The caller will guarantee to only call this function on a source in the
//  PLAYING state with a non-zero buffer, and will alter the source's state
//  to STOPPED when all buffers are processed. alBufferData is responsible for
//  associating a buffer with the right mixing function based on the format
//  of the audio data. This function only needs to concern itself with
//  correctly rendering the right part of the buffer and reporting how much
//  overflow is left for the next buffer in the queue to deal with.
typedef UInt32 (*__alMixFunc)(struct ALcontext_struct *ctx,
                               struct ALbuffer_struct *buf,
                               struct ALsource_struct *src,
                               Float32 *dst, UInt32 frames);

UInt32 __alMixMono8(struct ALcontext_struct *ctx,
                    struct ALbuffer_struct *buf,
                    struct ALsource_struct *src,
                    Float32 *dst, UInt32 frames);

UInt32 __alMixMono16(struct ALcontext_struct *ctx,
                     struct ALbuffer_struct *buf,
                     struct ALsource_struct *src,
                     Float32 *dst, UInt32 frames);

UInt32 __alMixStereo8(struct ALcontext_struct *ctx,
                      struct ALbuffer_struct *buf,
                      struct ALsource_struct *src,
                      Float32 *dst, UInt32 frames);

UInt32 __alMixStereo16(struct ALcontext_struct *ctx,
                       struct ALbuffer_struct *buf,
                       struct ALsource_struct *src,
                       Float32 *dst, UInt32 frames);

UInt32 __alMixMono8_altivec(struct ALcontext_struct *ctx,
                             struct ALbuffer_struct *buf,
                             struct ALsource_struct *src,
                             Float32 *dst, UInt32 frames);

UInt32 __alMixMono16_altivec(struct ALcontext_struct *ctx,
                             struct ALbuffer_struct *buf,
                             struct ALsource_struct *src,
                             Float32 *dst, UInt32 frames);

// !!! FIXME: No __alMixStereo8_altivec currently!

UInt32 __alMixStereo16_altivec(struct ALcontext_struct *ctx,
                               struct ALbuffer_struct *buf,
                               struct ALsource_struct *src,
                               Float32 *dst, UInt32 frames);


#if SUPPORTS_AL_EXT_FLOAT32
UInt32 __alMixMonoFloat32(struct ALcontext_struct *ctx,
                          struct ALbuffer_struct *buf,
                          struct ALsource_struct *src,
                          Float32 *dst, UInt32 frames);

UInt32 __alMixStereoFloat32(struct ALcontext_struct *ctx,
                            struct ALbuffer_struct *buf,
                            struct ALsource_struct *src,
                            Float32 *dst, UInt32 frames);

UInt32 __alMixMonoFloat32_altivec(struct ALcontext_struct *ctx,
                                  struct ALbuffer_struct *buf,
                                  struct ALsource_struct *src,
                                  Float32 *dst, UInt32 frames);

UInt32 __alMixStereoFloat32_altivec(struct ALcontext_struct *ctx,
                                    struct ALbuffer_struct *buf,
                                    struct ALsource_struct *src,
                                    Float32 *dst, UInt32 frames);
#endif


// resamplers ...
typedef ALboolean (*__alResampleFunc_t)(ALvoid*,ALsizei,ALvoid*,ALsizei);

ALboolean __alResampleMono8(ALvoid *_src, ALsizei _srcsize,
                            ALvoid *_dst, ALsizei _dstsize);
ALboolean __alResampleStereo8(ALvoid *_src, ALsizei _srcsize,
                              ALvoid *_dst, ALsizei _dstsize);
ALboolean __alResampleMono16(ALvoid *_src, ALsizei _srcsize,
                             ALvoid *_dst, ALsizei _dstsize);
ALboolean __alResampleStereo16(ALvoid *_src, ALsizei _srcsize,
                               ALvoid *_dst, ALsizei _dstsize);
ALboolean __alResampleMonoFloat32(ALvoid *_src, ALsizei _srcsize,
                                  ALvoid *_dst, ALsizei _dstsize);
ALboolean __alResampleStereoFloat32(ALvoid *_src, ALsizei _srcsize,
                                    ALvoid *_dst, ALsizei _dstsize);
ALboolean __alResampleSimpleMemcpy(ALvoid *_src, ALsizei _srcsize,
                                   ALvoid *_dst, ALsizei _dstsize);

// converters ...
ALboolean __alConvertFromMonoFloat32(Float32 *s, UInt8 *d,
                                     ALenum fmt, ALsizei samples);
ALboolean __alConvertFromStereoFloat32(Float32 *s, UInt8 *d,
                                       ALenum fmt, ALsizei samples);

// buffers...
typedef struct ALbuffer_struct 
{
	ALenum format;
	ALsizei frequency;
	ALsizei bits;
	ALsizei channels;
    ALboolean compressed;
    ALvoid *mixData;  // guaranteed to be 16-byte aligned.
    ALvoid *mixUnalignedPtr;  // malloc() retval used to calculate mixData.
    ALboolean inUse;
    ALsizei allocatedSpace;
    __alMixFunc mixFunc;

    // Stuff needed for some special formats...

    // Called when a buffer is about to be played on a source.
    //  allocate source-instance data and store in src->opaque.
    // Return AL_FALSE on a fatal error, AL_TRUE on success.
    ALboolean (*prepareBuffer)(struct ALcontext_struct *,
                               struct ALsource_struct *,
                               struct ALbuffer_struct *);

    // called when source is done playing this buffer and isn't looping.
    //  Should free up source-instance data here.
    ALvoid (*processedBuffer)(struct ALsource_struct *,
                              struct ALbuffer_struct *);

    // called if a source is looping this buffer. Prepare to replay from
    //  start on next call to mixFunc.
    ALvoid (*rewindBuffer)(struct ALsource_struct *, struct ALbuffer_struct *);

    // called during alDeleteBuffer(). Free up buffer-instance data.
    ALvoid (*deleteBuffer)(struct ALbuffer_struct *buf);
} ALbuffer;

ALvoid __alBuffersInit(ALbuffer *bufs, ALsizei count);
ALvoid __alBuffersShutdown(ALbuffer *buf, ALsizei count);

// sources...
typedef struct ALsource_struct
{
    ALboolean inUse;  // has been generated.
    ALvector Position;
    ALvector Velocity;
    ALuint state;
    ALboolean srcRelative;
    ALboolean looping;
    ALfloat pitch;
    ALfloat gain;
    ALfloat maxDistance;
    ALfloat minGain;
    ALfloat maxGain;
    ALfloat rolloffFactor;
    ALfloat referenceDistance;

    // Buffer Queues...
    ALuint bufferQueue[AL_MAXBUFFERQUEUE];
    ALuint bufferPos;
    ALuint bufferCount;
    ALuint bufferReadIndex;

    // Current buffer's scratch space...vorbis, etc use this.
    void *opaque;

    // Mixing (p)recalculation...
    ALboolean needsRecalculation;

    // A positioned source won't ever be played on more than 3 channels
    //  (3 in VBAP3D, 2 in stereo and VBAP2D).
    ALsizei channel0;
    ALsizei channel1;
    ALsizei channel2;
    ALfloat channelGain0;
    ALfloat channelGain1;
    ALfloat channelGain2;
    ALfloat channelgains[AL_MAXSPEAKERS];
} ALsource;

ALvoid __alSourcesInit(ALsource *src, ALsizei count);
ALvoid __alSourcesShutdown(ALsource *src, ALsizei count);


// The Listener...
typedef struct ALlistener_struct
{
    ALvector Position;
    ALvector Velocity;
	ALvector Forward;
	ALvector Up;
	ALfloat Gain;
	ALint Environment;
} ALlistener;

ALvoid __alListenerInit(ALlistener *listener);
ALvoid __alListenerShutdown(ALlistener *listener);


// Context/device stuff...

typedef struct ALcontext_struct
{
    ALboolean inUse;
    ALboolean suspended;
    ALlock contextLock;
    struct ALdevice_struct *device;
    ALlistener listener;
    ALenum distanceModel;
    ALfloat dopplerFactor;
    ALfloat dopplerVelocity;
    ALfloat propagationSpeed;

    ALsource sources[AL_MAXSOURCES];
    ALsizei generatedSources;

    // pointers into (sources) to prevent cache thrashing.
    ALsource *playingSources[AL_MAXSOURCES+1]; // +1 so it's null term'd.

    // !!! FIXME: According to the AL spec:
    // !!! FIXME:   "Unlike Sources and Listener, Buffer Objects can be shared
    // !!! FIXME:    among AL contexts."
    // !!! FIXME:
    // !!! FIXME: I take this to imply, "but don't have to be," but that's
    // !!! FIXME:  because I don't want to redesign and verify all my code
    // !!! FIXME:  for the new thread safety issues in the short term.
    // !!! FIXME:  Eventually, these should make it at least to device
    // !!! FIXME:  resolution, or perhaps globally if mutexes aren't a pain
    // !!! FIXME:  to implement well.
    ALbuffer buffers[AL_MAXBUFFERS];
} ALcontext;

ALcontext *__alGrabCurrentContext(ALvoid);
ALvoid __alUngrabContext(ALcontext *ctx);


typedef enum
{
    SPKCFG_STDSTEREO, // 2 speakers, left and right. Regular stereo output.
    SPKCFG_POSATTENUATION, // 2D position attenuation
    #if USE_VBAP
    SPKCFG_VBAP_2D,   // >= 2 speakers around listener at elevation 0.
    SPKCFG_VBAP_3D,   // >= 3 speakers around listener at arbitrary positions.
    #endif
} SpeakerConfigType;

typedef struct ALdevice_struct
{
    #if MACOSX
    AudioDeviceID device;
    AudioStreamBasicDescription	streamFormat;
    #endif

    #if SUPPORTS_ALC_EXT_CAPTURE
    ALboolean isInputDevice;
    struct   // put this in a struct for the sake of the namespace...
    {
        ALboolean started;
        ALenum format;
        ALuint formatSize;
        ALuint freq;
        __ALRingBuffer ring;
        UInt8 *resampled;
        UInt8 *converted;
    } capture;
    #endif

    ALenum errorCode;
    ALlock deviceLock;
    SpeakerConfigType speakerConfig;
    ALint speakers;  // !!! FIXME: "speakers" isn't right, since capture devices don't have speakers. "Channels" is better.
    ALfloat speakergains[AL_MAXSPEAKERS];
    ALfloat speakerazimuths[AL_MAXSPEAKERS];
    ALfloat speakerelevations[AL_MAXSPEAKERS];
    ALfloat speakerpos[AL_MAXSPEAKERS*3]; // x, y, z  ... internal use only.

    #if USE_VBAP
    ALsizei vbapgroups;  // number of speaker triangles/pairs.
    ALsizei vbapsorted[AL_MAXSPEAKERS*3]; // triangulated speakers sort order.
    ALfloat vbapmatrix[AL_MAXSPEAKERS*9];  // triangulated speakers matrix.
    #endif

    ALcontext contexts[AL_MAXCONTEXTS];   // static context storage.
    // pointers into (contexts) to prevent cache thrashing.
    ALcontext *createdContexts[AL_MAXCONTEXTS+1]; // +1 so it's null-term'd.
} ALdevice;


// other stuff...

#define DST_BLOCK_CTRL(size,count,stride) \
            (((size) << 24) | ((count) << 16) | (stride))

ALvoid __alSetError(ALenum err);
ALboolean __alDetectVectorUnit(ALvoid);
ALvoid __alRecalcMonoSource(ALcontext *ctx, ALsource *src);

// Can be AL_TRUE after first context creation, but toggled off through
//  extension. If you need a definitive answer, use __alDetectVectorUnit().
extern ALboolean __alHasEnabledVectorUnit;

// VBAP stuff.
#if USE_VBAP
ALvoid __alcTriangulateSpeakers(ALdevice *device);
ALvoid __alDoVBAP(ALcontext *ctx, ALsource *src, ALvector pos, ALfloat gain);
#endif

// These are only in the win32 branch's headers right now for some reason...
#ifndef AL_INVALID_ENUM
#warning AL_INVALID_ENUM not defined! Fix the global headers!
#define AL_INVALID_ENUM                          0xA002  
#endif

#ifndef AL_INVALID_OPERATION
#warning AL_INVALID_OPERATION not defined! Fix the global headers!
#define AL_INVALID_OPERATION                     0xA004
#endif


// Extensions...
const ALubyte *__alCalculateExtensions(ALboolean isALC);
ALboolean __alIsExtensionPresent(const ALubyte *extName, ALboolean isALC);


#if SUPPORTS_AL_EXT_VECTOR_UNIT
  #ifndef AL_VECTOR_UNIT_EXT
    #define AL_VECTOR_UNIT_EXT 0x3100
  #endif
#endif // SUPPORTS_AL_EXT_VECTOR_UNIT


#if SUPPORTS_AL_EXT_VORBIS
  #ifndef AL_FORMAT_VORBIS_EXT
    #define AL_FORMAT_VORBIS_EXT 0x10003
  #endif

  ALvoid __alBufferDataFromVorbis(ALbuffer *buf);
#endif // SUPPORTS_AL_EXT_VORBIS

#if SUPPORTS_AL_EXT_MP3
  #ifndef AL_FORMAT_MP3_EXT
    #define AL_FORMAT_MP3_EXT 0x10004
  #endif

  ALvoid __alBufferDataFromMP3(ALbuffer *buf);
#endif // SUPPORTS_AL_EXT_MP3

#if SUPPORTS_AL_EXT_BUFFER_OFFSET
  #ifndef AL_BUFFER_OFFSET_EXT
    #define AL_BUFFER_OFFSET_EXT 0x1019
  #endif
#endif


#if SUPPORTS_ALC_EXT_SPEAKER_ATTRS
  ALCAPI ALvoid ALCAPIENTRY alcGetSpeakerfv(ALCdevice *dev, ALuint spk,
                                            ALenum param, ALfloat *v);
  ALCAPI ALvoid ALCAPIENTRY alcSpeakerfv(ALCdevice *dev, ALuint spk,
                                         ALfloat *v);
  ALCAPI ALvoid ALCAPIENTRY alcSpeakerf(ALCdevice *dev, ALuint spk, ALfloat x);

  #ifndef ALC_SPEAKER_COUNT
    #define ALC_SPEAKER_COUNT       0x1032  // alcGetInteger
  #endif
  #ifndef ALC_SPEAKER_GAIN
    #define ALC_SPEAKER_GAIN        0x1033  // alcSpeakerf
  #endif
  #ifndef ALC_SPEAKER_AZIMUTH
    #define ALC_SPEAKER_AZIMUTH     0x1034  // alcSpeakerf
  #endif
  #ifndef ALC_SPEAKER_ELEVATION
    #define ALC_SPEAKER_ELEVATION   0x1035  // alcSpeakerf
  #endif
#endif

#if SUPPORTS_AL_EXT_FLOAT32
  #ifndef AL_FORMAT_MONO_FLOAT32
    #define AL_FORMAT_MONO_FLOAT32  0x10010
  #endif
  #ifndef AL_FORMAT_STEREO_FLOAT32
    #define AL_FORMAT_STEREO_FLOAT32  0x10011
  #endif
#endif

#if SUPPORTS_ALC_EXT_CAPTURE
  #ifndef ALC_CAPTURE_DEVICE_SPECIFIER
    #define ALC_CAPTURE_DEVICE_SPECIFIER 0x310
  #endif
  #ifndef ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER
    #define ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER 0x311
  #endif
  #ifndef ALC_CAPTURE_SAMPLES
    #define ALC_CAPTURE_SAMPLES 0x312
  #endif

  ALCAPI ALCdevice* ALCAPIENTRY alcCaptureOpenDevice(const ALubyte *deviceName,
                                                     ALuint freq, ALenum fmt,
                                                     ALsizei bufsize);
  ALCAPI ALvoid ALCAPIENTRY alcCaptureCloseDevice(ALCdevice *device);
  ALCAPI ALvoid ALCAPIENTRY alcCaptureStart(ALCdevice *device);
  ALCAPI ALvoid ALCAPIENTRY alcCaptureStop(ALCdevice *device);
  ALCAPI ALvoid ALCAPIENTRY alcCaptureSamples(ALCdevice *device, ALvoid *buf,
                                              ALsizei samps);
#endif

#endif // include-once blocker.

// end of alInternal.h ...

