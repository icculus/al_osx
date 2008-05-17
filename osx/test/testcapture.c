/*
 * This is a test program for the ALC_EXT_capture extension. This
 *  test code is public domain and comes with NO WARRANTY.
 *
 * Written by Ryan C. Gordon <icculus@icculus.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "al.h"
#include "alc.h"


static ALCdevice* (*palcCaptureOpenDevice)(const ALubyte *deviceName,
                                     ALuint freq, ALenum fmt,
                                     ALsizei bufsize);
static ALvoid (*palcCaptureCloseDevice)(ALCdevice *device);
static ALvoid (*palcCaptureStart)(ALCdevice *device);
static ALvoid (*palcCaptureStop)(ALCdevice *device);
static ALvoid (*palcCaptureSamples)(ALCdevice *device, ALvoid *buf,
                                   ALsizei samps);

ALenum _AL_FORMAT_MONO_FLOAT32 = 0;
ALenum _AL_FORMAT_STEREO_FLOAT32 = 0;

#define FMT AL_FORMAT_MONO16
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
    palcCaptureStart(in);

    while (samples < SAMPS)
    {
        alcGetIntegerv(in, capture_samples, sizeof (samples), &samples);
        usleep(10000);
    } // while

    palcCaptureSamples(in, buf, SAMPS);
    palcCaptureStop(in);
    return(buf);  // buf has SAMPS samples of FMT audio at FREQ.
} // recordSomething


int main(int argc, char **argv)
{
    ALboolean ext;
    ALuint sid;
    ALuint bid;
    ALint state = AL_PLAYING;

    if (!alcIsExtensionPresent(NULL, "ALC_EXT_capture"))
        fprintf(stderr, "No ALC_EXT_capture support reported.\n");

    #define GET_PROC(x) p##x = alcGetProcAddress(NULL, (ALubyte *) #x)
    GET_PROC(alcCaptureOpenDevice);
    GET_PROC(alcCaptureCloseDevice);
    GET_PROC(alcCaptureStart);
    GET_PROC(alcCaptureStop);
    GET_PROC(alcCaptureSamples);

    if (palcCaptureOpenDevice == NULL)
    {
        fprintf(stderr, "entry points missing, extension really doesn't exist.\n");
        return 42;   // it really doesn't exist, bail.
    } // if

    // these may not exist, depending on the implementation.
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
    in = palcCaptureOpenDevice(NULL, FREQ, FMT, SAMPS);
    if (in == NULL)
    {
        fprintf(stderr, "Couldn't open capture device.\n");
        alutExit();
        return(42);
    } // if

    recordSomething();

    palcCaptureCloseDevice(in);

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

