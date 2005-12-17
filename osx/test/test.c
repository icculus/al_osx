#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "AL/al.h"
#include "AL/alc.h"


#define LOAD_VORBIS 0
#define LOAD_MP3 0
#define LOAD_WAV 1
#define TEST_BUFFER_DATA 0
#define DUMP_WAV 0
#define TEST_AL_EXT_BUFFER_OFFSET 0
#define TEST_SPATIALIZATION 0
#define TEST_LOOPING 0

int main(int argc, char **argv)
{
    FILE *io;
    ALCdevice *dev = NULL;
    ALCcontext *ctx = NULL;
    ALenum format;
    ALvoid *data;
    ALsizei size;
    ALsizei freq;
    ALboolean loop;
    ALuint sid;
    ALuint bid;
    ALint state = AL_PLAYING;
    ALsizei i;
    ALenum alenum;
    ALboolean ext;
    ALint bits;
    ALint channels;

    #if TEST_SPATIALIZATION
    ALfloat pos3d[3];
    ALfloat posoffset = 0.10f;
    ALsizei axis = 0;
    #endif

    #define printALCString(dev, ext) { ALenum e = alcGetEnumValue(dev, #ext); printf("%s: %s\n", #ext, alcGetString(dev, e)); }
    printALCString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
    printALCString(NULL, ALC_EXTENSIONS);

    ext = alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT");
    if (!ext)
        printf("No ALC_ENUMERATION_EXT support.\n");
    else
    {
        char *devList;
        alenum = alcGetEnumValue(NULL, "ALC_DEVICE_SPECIFIER");
		devList = (char *)alcGetString(NULL, alenum);

        printf("ALC_ENUMERATION_EXT:\n");
        while (*devList)  // I really hate this double null terminated list thing.
        {
            printf("  - %s\n", devList);
            devList += strlen(devList) + 1;
        } // while
    } // else

    dev = alcOpenDevice(getenv("OPENAL_DEVICE"));  // getenv()==NULL is okay.
 	if (dev != NULL)
 	{
		ctx = alcCreateContext(dev, 0);
		if (ctx != NULL)
        {
			alcMakeContextCurrent(ctx);
            alcProcessContext(ctx);
        } // if
	} // if

    if (ctx == NULL)
    {
        printf("Don't have an AL context. Aborting.\n");
        if (dev)
            alcCloseDevice(dev);
        return(0);
    } // if

    alGenSources(1, &sid);
    alGenBuffers(1, &bid);

    #define printALString(ext) { ALenum e = alGetEnumValue(#ext); printf("%s: %s\n", #ext, alGetString(e)); }
    printALString(AL_RENDERER);
    printALString(AL_VERSION);
    printALString(AL_VENDOR);
    printALString(AL_EXTENSIONS);

    if (getenv("NO_ALTIVEC"))
    {
        if (alIsExtensionPresent("AL_EXT_vector_unit"))
        {
            printf("Disabling vector unit...\n");
            alenum = alGetEnumValue("AL_VECTOR_UNIT_EXT");
            alDisable(alenum);
        } // if
    } // if

#if LOAD_VORBIS
  {
    struct stat statbuf;
    if (!alIsExtensionPresent("AL_EXT_vorbis"))
    {
        printf("No vorbis support.\n");
        return(0);
    } // if

    alenum = alGetEnumValue("AL_FORMAT_VORBIS_EXT");
    io = fopen("sample.ogg", "rb");
    if (io == NULL) { printf("file open failed.\n"); return(0); }
    fstat(fileno(io), &statbuf);
    size = statbuf.st_size;
    data = malloc(size);
    fread(data, size, 1, io);
    fclose(io);
    loop = AL_TRUE;

    #if TEST_BUFFER_DATA
    for (i = 0; i < 5000; i++)
    #endif
        alBufferData(bid, alenum, data, size, 0);
  }
#elif LOAD_MP3
  {
    struct stat statbuf;
    if (!alIsExtensionPresent("AL_EXT_mp3"))
    {
        printf("No mp3 support.\n");
        return(0);
    } // if

    alenum = alGetEnumValue("AL_FORMAT_MP3_EXT");
    io = fopen("sample.mp3", "rb");
    if (io == NULL) { printf("file open failed.\n"); return(0); }
    fstat(fileno(io), &statbuf);
    size = statbuf.st_size;
    data = malloc(size);
    fread(data, size, 1, io);
    fclose(io);
    loop = AL_TRUE;

    #if TEST_BUFFER_DATA
    for (i = 0; i < 5000; i++)
    #endif
        alBufferData(bid, alenum, data, size, 0);
  }
#else
  #if LOAD_WAV
    // !!! FIXME: Arguably, this is illegal without alutInit().
    alutLoadWAVFile("sample.wav", &format, &data, &size, &freq, &loop);
  #else
  {
    struct stat statbuf;
    io = fopen("2.raw", "rb");
    if (io == NULL) { printf("file open failed.\n"); return(0); }
    fstat(fileno(io), &statbuf);
    size = statbuf.st_size;
    data = malloc(size);
    fread(data, size, 1, io);
    fclose(io);
    format = alGetEnumValue((ALubyte *) "AL_FORMAT_MONO_FLOAT32");
    freq = 44100;
    loop = AL_FALSE;
  }
 #endif

 #if DUMP_WAV
    io = fopen("decoded.raw", "wb");
    fwrite(data, size, 1, io);
    fclose(io);
 #endif

 #if TEST_BUFFER_DATA
    for (i = 0; i < 1000; i++)
 #endif
        alBufferData(bid, format, data, size, freq);


 #if LOAD_WAV
    // !!! FIXME: Arguably, this is illegal without alutInit().
    alutUnloadWAV(format, data, size, freq);
 #else
    free(data);
 #endif
#endif

    alGetBufferi(bid, AL_BITS, &bits);
    alGetBufferi(bid, AL_CHANNELS, &channels);
    alGetBufferi(bid, AL_FREQUENCY, &freq);

#if TEST_LOOPING
    loop = AL_TRUE;
#endif

#if !TEST_BUFFER_DATA
    alSourcei(sid, AL_BUFFER, bid);
    alSourcei(sid, AL_LOOPING, loop);
    alSourcePlay(sid);

    #if TEST_AL_EXT_BUFFER_OFFSET
    ext = alIsExtensionPresent("AL_EXT_buffer_offset");
    if (ext)
    {
        ALint thirtysecs = 30 * ((bits / 8) * channels * freq);
        alenum = alGetEnumValue("AL_BUFFER_OFFSET_EXT");
        alGetError();  // clear previous errors.

        // jump ahead 30 seconds.
        alSourcei(sid, alenum, thirtysecs);
        printf("Jump 30 secs (%d bytes) via AL_EXT_buffer_offset: error is %d\n",
                (int) thirtysecs, (int) alGetError());
    } // if
    #endif

    while (state == AL_PLAYING)
    {
        #if TEST_AL_EXT_BUFFER_OFFSET
        ALint pos;
        if (ext)
        {
            alGetSourcei(sid, alenum, &pos);
            printf("AL_EXT_buffer_offset: Current byte offset: %d\n", (int) pos);
        } // if
        #endif

        #if TEST_SPATIALIZATION
        alGetSourcefv(sid, AL_POSITION, pos3d);

        pos3d[axis] += posoffset;

        if (posoffset > 0.0f)
        {
            if (pos3d[axis] > 1.8f)
            {
                axis += 2;
                if (axis >= 3)
                {
                    posoffset = -posoffset;
                    axis = 0;
                }
            }
        }
        else
        {
            if (pos3d[axis] < -1.8f)
            {
                axis += 2;
                if (axis >= 3)
                {
                    ALfloat l_or[6];
                    alGetListenerfv(AL_ORIENTATION, l_or);
                    l_or[2] = -l_or[2];
                    alListenerfv(AL_ORIENTATION, l_or);
                    printf("flipped listener!\n");
                    posoffset = -posoffset;
                    axis = 0;
                }
            }
        }

        printf("%f %s  %f %s  %f %s\n",
                pos3d[0], pos3d[0] < 0.0f ? "left" : "right",
                pos3d[1], pos3d[1] < 0.0f ? "down" : "up",
                pos3d[2], pos3d[2] < 0.0f ? "back" : "front");
        alSourcefv(sid, AL_POSITION, pos3d);
        #endif

        usleep(100000);
        alGetSourcei(sid, AL_SOURCE_STATE, &state);
    } // while
#endif

    alDeleteSources(1, &sid);
    alDeleteBuffers(1, &bid);

    alcSuspendContext(ctx);
    alcMakeContextCurrent(NULL);
    alcDestroyContext(ctx);
    alcCloseDevice(dev);

    return(0);
} // main

// end of test.c ...

