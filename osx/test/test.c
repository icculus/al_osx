#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "AL/al.h"
#include "AL/alc.h"
#include "AL/alut.h"

int main(int argc, char **argv)
{
    FILE *io;
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

    alutInit(&argc, (ALbyte **) argv);
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

#define LOAD_VORBIS 1
#define LOAD_WAV 0
#define TEST_BUFFER_DATA 0
#define DUMP_WAV 0
#define TEST_AL_EXT_BUFFER_OFFSET 0

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
#else
  #if LOAD_WAV
    alutLoadWAVFile("sample.wav", &format, &data, &size, &freq, &loop);
  #else
  {
    struct stat statbuf;
    io = fopen("sdlaudio.raw", "rb");
    if (io == NULL) { printf("file open failed.\n"); return(0); }
    fstat(fileno(io), &statbuf);
    size = statbuf.st_size;
    data = malloc(size);
    fread(data, size, 1, io);
    fclose(io);
    format = AL_FORMAT_STEREO8;
    freq = 22050;
    loop = AL_TRUE;
  }
 #endif

 #if DUMP_WAV
    io = fopen("decoded.raw", "wb");
    fwrite(data, size, 1, io);
    fclose(io);
 #endif

 #if TEST_BUFFER_DATA
    for (i = 0; i < 5000; i++)
 #endif
        alBufferData(bid, format, data, size, freq);


 #if LOAD_WAV
    alutUnloadWAV(format, data, size, freq);
 #else
    free(data);
 #endif
#endif

    alGetBufferi(bid, AL_BITS, &bits);
    alGetBufferi(bid, AL_CHANNELS, &channels);
    alGetBufferi(bid, AL_FREQUENCY, &freq);

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

        sleep(1);
        alGetSourcei(sid, AL_SOURCE_STATE, &state);
    } // while
#endif

    alDeleteSources(1, &sid);
    alDeleteBuffers(1, &bid);
    alutExit();
    return(0);
} // main

// end of test.c ...

