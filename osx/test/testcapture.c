#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "AL/al.h"
#include "AL/alc.h"


static ALCdevice* (*alcCaptureOpenDevice)(const ALubyte *deviceName,
                                     ALuint freq, ALenum fmt,
                                     ALsizei bufsize);
static ALvoid (*alcCaptureCloseDevice)(ALCdevice *device);
static ALvoid (*alcCaptureStart)(ALCdevice *device);
static ALvoid (*alcCaptureStop)(ALCdevice *device);
static ALvoid (*alcCaptureSamples)(ALCdevice *device, ALvoid *buf,
                                   ALsizei samps);

ALenum _AL_FORMAT_MONO_FLOAT32 = 0;
ALenum _AL_FORMAT_STEREO_FLOAT32 = 0;

#define FMT _AL_FORMAT_STEREO_FLOAT32
#define FMTSIZE 8
#define FREQ 44100
#define SAMPS (FREQ * 5)
static ALbyte buf[SAMPS * FMTSIZE];
static ALCdevice *in = NULL;

ALvoid *recordSomething(void)
{
    ALint samples = 0;
    ALenum capture_samples = alcGetEnumValue(in, "ALC_CAPTURE_SAMPLES");
    printf("recording...\n");
    alcCaptureStart(in);

    while (samples < SAMPS)
    {
        alcGetIntegerv(in, capture_samples, sizeof (samples), &samples);
        usleep(10000);
    } // while

    alcCaptureSamples(in, buf, SAMPS);
    alcCaptureStop(in);
    return(buf);  // buf has SAMPS samples of FMT audio at FREQ.
} // recordSomething


int main(int argc, char **argv)
{
    ALboolean ext;
    ALuint sid;
    ALuint bid;
    ALint state = AL_PLAYING;

    if (!alcIsExtensionPresent(NULL, "ALC_EXT_capture"))
    {
        fprintf(stderr, "No ALC_EXT_capture support.\n");
        return(42);
    } // if

    #define GET_PROC(x) x = alcGetProcAddress(NULL, (ALubyte *) #x)
    GET_PROC(alcCaptureOpenDevice);
    GET_PROC(alcCaptureCloseDevice);
    GET_PROC(alcCaptureStart);
    GET_PROC(alcCaptureStop);
    GET_PROC(alcCaptureSamples);

    _AL_FORMAT_MONO_FLOAT32 = alGetEnumValue((ALubyte *) "AL_FORMAT_MONO_FLOAT32");
    _AL_FORMAT_STEREO_FLOAT32 = alGetEnumValue((ALubyte *) "AL_FORMAT_STEREO_FLOAT32");
    alGetError();

    #define printALCString(dev, ext) { ALenum e = alcGetEnumValue(dev, #ext); printf("%s: %s\n", #ext, alcGetString(dev, e)); }
    printALCString(NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);

    ext = alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT");
    if (!ext)
        printf("No ALC_ENUMERATION_EXT support.\n");
    else
    {
        char *devList;
        ALenum alenum = alcGetEnumValue(NULL, "ALC_CAPTURE_DEVICE_SPECIFIER");
		devList = (char *)alcGetString(NULL, alenum);

        printf("ALC_ENUMERATION_EXT:\n");
        while (*devList)  // I really hate this double null terminated list thing.
        {
            printf("  - %s\n", devList);
            devList += strlen(devList) + 1;
        } // while
    } // else

    alutInit(&argc, argv);
    in = alcCaptureOpenDevice(NULL, FREQ, FMT, SAMPS);
    if (in == NULL)
    {
        fprintf(stderr, "Couldn't open capture device.\n");
        alutExit();
        return(42);
    } // if

    recordSomething();

    alcCaptureCloseDevice(in);

    alGenSources(1, &sid);
    alGenBuffers(1, &bid);

    printf("Playing...\n");

    alBufferData(bid, FMT, buf, sizeof (buf), FREQ);
    alSourcei(sid, AL_BUFFER, bid);
    alSourcePlay(sid);

    while (state == AL_PLAYING)
    {
        usleep(100000);
        alGetSourcei(sid, AL_SOURCE_STATE, &state);
    } // while

    printf("Cleaning up...\n");

    alDeleteSources(1, &sid);
    alDeleteBuffers(1, &bid);

    alutExit();
    return(0);
} // main

// end of testcapture.c ...

