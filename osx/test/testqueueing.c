#include <stdio.h>
#include <stdlib.h>
#include "AL/al.h"
#include "AL/alut.h"

#define MAX_QUEUE_BUFFERS 32
int main(int argc, char **argv)
{
    int buf_ptr = 0;
    FILE *io;
    ALenum format;
    short data[8192];
    ALsizei size;
    ALsizei freq;
    ALboolean loop;
    ALuint sid;
    ALuint bid[MAX_QUEUE_BUFFERS];
    ALint state = AL_PLAYING;
    ALsizei i;
    int rc = 0;
    ALint processed;
    ALint queued;

    io = fopen("pirates.raw", "rb");
    if (!io)
        return(0);

    alutInit(&argc, (ALbyte **) argv);
    alGenSources(1, &sid);
    alGenBuffers(MAX_QUEUE_BUFFERS, bid);

    while (!feof(io))
    {
        alGetSourcei(sid, AL_BUFFERS_QUEUED, &queued);
        alGetSourcei(sid, AL_BUFFERS_PROCESSED, &processed);
        if (processed)
        {
            //printf("%d buffers processed.\n", (int) processed);
            ALint i;
            int ptr = buf_ptr - queued;
            if (ptr < 0)
                ptr += MAX_QUEUE_BUFFERS;

            for (i = 0; i < processed; i++)
            {
                alSourceUnqueueBuffers(sid, 1, &bid[ptr]);
                printf("Unqueued buffer %d\n", (int) bid[ptr]);
                if (++ptr >= MAX_QUEUE_BUFFERS)
                    ptr = 0;
            }
        }

        alGetSourcei(sid, AL_SOURCE_STATE, &state);
        alGetSourcei(sid, AL_BUFFERS_QUEUED, &queued);
        if (queued < MAX_QUEUE_BUFFERS)
        {
            //printf("only %d of %d buffers queued.\n", (int) queued, MAX_QUEUE_BUFFERS);
            rc = fread(data, 1, sizeof (data), io);
            if (rc > 0)
            {
                printf("buffering %d bytes to bid %d\n", rc, (int) bid[buf_ptr]);
                alBufferData(bid[buf_ptr], AL_FORMAT_STEREO16, data, rc, 44100);
                alSourceQueueBuffers(sid, 1, &bid[buf_ptr]);
                buf_ptr++;
                buf_ptr %= MAX_QUEUE_BUFFERS;
            }
	    }

	    if(state != AL_PLAYING) {
                printf("calling alSourcePlay...\n");
			    alSourcePlay(sid);
	    }
    } // while

    printf("eof.\n");

    fclose(io);

    while (state == AL_PLAYING)
    {
        sleep(1);
        alGetSourcei(sid, AL_SOURCE_STATE, &state);
    } // while

    printf("all buffers processed.\n");

    alDeleteSources(1, &sid);
    alDeleteBuffers(MAX_QUEUE_BUFFERS, bid);
    alutExit();

    return(0);
} // main

// end of test.c ...

