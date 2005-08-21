/*
 * This is a test program for the ALC_EXT_disconnect extension. This
 *  test code is public domain and comes with NO WARRANTY.
 *
 * Written by Ryan C. Gordon <icculus@icculus.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "AL/al.h"
#include "AL/alc.h"

#ifndef ALC_CONNECTED
#define ALC_CONNECTED 0x313
#endif

#define DO_CAPTURE_INSTEAD 0
#if DO_CAPTURE_INSTEAD
  ALCAPI ALCdevice* ALCAPIENTRY alcCaptureOpenDevice(const ALubyte *deviceName,
                                                     ALuint freq, ALenum fmt,
                                                     ALsizei bufsize);
  ALCAPI ALvoid ALCAPIENTRY alcCaptureCloseDevice(ALCdevice *device);
#  define alcOpenDevice(x) alcCaptureOpenDevice(x, 44100, AL_FORMAT_MONO16, 44100*5)
#  define alcCloseDevice(x) alcCaptureCloseDevice(x)
#endif

static ALCdevice *open_device(const char *devname)
{
    ALenum alenum = 0;
    const char *printname = devname ? devname : "[DEFAULT]";
    ALCdevice *retval = alcOpenDevice(devname);
    if (retval == NULL)
        fprintf(stderr, "failed to open device '%s'\n", printname);
    else
    {
        printf("Device '%s' opened (%p).\n", printname, retval);
        if (!alcIsExtensionPresent(retval, "ALC_EXT_disconnect"))
        {
            fprintf(stderr, "No ALC_EXT_disconnect support on '%s'.\n", printname);
            alcCloseDevice(retval);
            return(NULL);
        } // if

        alGetError();
        alenum = alcGetEnumValue(retval, (ALubyte *) "ALC_CONNECTED");
        if (alGetError())
        {
            fprintf(stderr, "ALC_EXT_disconnect supported on '%s' but"
                            " alcGetEnumValue(\"ALC_CONNECTED\") fails.\n",
                            printname);
            alcCloseDevice(retval);
            return(NULL);
        } // if
        if (alenum != 0x313)
        {
            fprintf(stderr, "ALC_EXT_disconnect supported on '%s' but"
                            " alcGetEnumValue(\"ALC_CONNECTED\") is wrong.\n"
                            "  (%d instead of %d).\n",
                            printname, (int) alenum, 0x313);
            alcCloseDevice(retval);
            return(NULL);
        } // if
    } // else

    return(retval);
} // open_device

int main(int argc, char **argv)
{
    #define MAXDEVS 128
    ALCdevice *devs[MAXDEVS];
    ALsizei devcount = 0;
    int i;

    for (i = 1; i < argc; i++)
    {
        ALCdevice *dev = open_device(argv[i]);
        if (dev)
        {
            devs[devcount++] = dev;
            if (devcount >= MAXDEVS)
            {
                printf("...that's enough devices!\n");
                break;
            } // if
        } // else
    } // for

    if (devcount == 0)
    {
        printf("No devices opened...trying default device...\n");
        devs[0] = open_device(NULL);
        if (devs[0])
            devcount++;
    } // if

    int devsleft = devcount;
    while (devsleft)
    {
        sleep(1);
        devsleft = 0;
        for (i = 0; i < devcount; i++)
        {
            if (!devs[i])
                continue;

            ALint alive = 0;
            alcGetIntegerv(devs[i], ALC_CONNECTED, sizeof (alive), &alive);
            if (alive)
                devsleft++;
            else
            {
                printf("device %p is no longer connected.\n", devs[i]);
                alcCloseDevice(devs[i]);
                devs[i] = NULL;
            } // else
        } // for
    } // while

    printf("No open devices left. Terminating...\n");
    return(0);
} // main

// end of testcapture.c ...

