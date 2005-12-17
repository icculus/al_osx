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

// !!! FIXME: This needs updating.

void convert_c2pstr(char *string);
void SwapWords(unsigned int *puint);
void SwapBytes(unsigned short *pshort);

//#pragma pack (push,1) 							/* Turn off alignment */

typedef struct                                  /* WAV File-header */
{
  ALubyte  Id[4];
  ALsizei  Size;
  ALubyte  Type[4];
} WAVFileHdr_Struct;

typedef struct                                  /* WAV Fmt-header */
{
  ALushort Format;                              
  ALushort Channels;
  ALuint   SamplesPerSec;
  ALuint   BytesPerSec;
  ALushort BlockAlign;
  ALushort BitsPerSample;
} WAVFmtHdr_Struct;

typedef struct									/* WAV FmtEx-header */
{
  ALushort Size;
  ALushort SamplesPerBlock;
} WAVFmtExHdr_Struct;

typedef struct                                  /* WAV Smpl-header */
{
  ALuint   Manufacturer;
  ALuint   Product;
  ALuint   SamplePeriod;                          
  ALuint   Note;                                  
  ALuint   FineTune;                              
  ALuint   SMPTEFormat;
  ALuint   SMPTEOffest;
  ALuint   Loops;
  ALuint   SamplerData;
  struct
  {
    ALuint Identifier;
    ALuint Type;
    ALuint Start;
    ALuint End;
    ALuint Fraction;
    ALuint Count;
  }      Loop[1];
} WAVSmplHdr_Struct;

typedef struct                                  /* WAV Chunk-header */
{
  ALubyte  Id[4];
  ALuint   Size;
} WAVChunkHdr_Struct;


#if HAVE_PRAGMA_EXPORT
#pragma export on
#endif

static ALboolean alut_initted = AL_FALSE;

ALUTAPI ALvoid ALUTAPIENTRY alutInit(ALint *argc,ALbyte **argv)
{
	ALCcontext *Context;
	ALCdevice *Device;
	
    if (alut_initted)
        return;

 	Device=alcOpenDevice(NULL);  //Open device
 	if (Device != NULL)
 	{
		Context=alcCreateContext(Device,0);  //Create context(s)
		
		if (Context != NULL)
        {
			alcMakeContextCurrent(Context);  //Set active context
            alcProcessContext(Context);
            alut_initted = AL_TRUE;
        } // if

        else
        {
            alcCloseDevice(Device);
        } // else
	} // if
} // alutInit


ALUTAPI ALvoid ALUTAPIENTRY alutExit(ALvoid) 
{
	ALCcontext *Context;
	ALCdevice *Device;

    if (!alut_initted)
        return;

	//Get active context
	Context=alcGetCurrentContext();
	//Get device for active context
	Device=alcGetContextsDevice(Context);

    alcSuspendContext(Context);
    alcMakeContextCurrent(NULL);

	//Release context(s)
	alcDestroyContext(Context);
	//Close device
	alcCloseDevice(Device);

    alut_initted = AL_FALSE;
}

void SwapWords(unsigned int *puint)
{
#if __POWERPC__
    unsigned int tempint;
	char *pChar1, *pChar2;
	
	tempint = *puint;
	pChar2 = (char *)&tempint;
	pChar1 = (char *)puint;
	
	pChar1[0]=pChar2[3];
	pChar1[1]=pChar2[2];
	pChar1[2]=pChar2[1];
	pChar1[3]=pChar2[0];
#endif
}

void SwapBytes(unsigned short *pshort)
{
#if __POWERPC__
    unsigned short tempshort;
    char *pChar1, *pChar2;
    
    tempshort = *pshort;
    pChar2 = (char *)&tempshort;
    pChar1 = (char *)pshort;
    
    pChar1[0]=pChar2[1];
    pChar1[1]=pChar2[0];
#endif
}

void convert_c2pstr(char *string)
{
	int length, i;
	
	length = strlen((const char *)string);
	if (length > 254) { length = 254; }
	for (i = length; i > 0; i--)
	{
		string[i] = string[i-1];
	}
	string[0] = length;
}
 
ALUTAPI ALvoid ALUTAPIENTRY alutLoadWAVFile(ALbyte *file,ALenum *format,ALvoid **data,ALsizei *size,ALsizei *freq,ALboolean *loop)
{
	WAVChunkHdr_Struct ChunkHdr;
	WAVFmtExHdr_Struct FmtExHdr;
	WAVFileHdr_Struct FileHdr;
	WAVSmplHdr_Struct SmplHdr;
	WAVFmtHdr_Struct FmtHdr;
	FILE *sfFile;
	long numBytes;
	int i;
	
	*format=AL_FORMAT_MONO16;
	*data=NULL;
	*size=0;
	*freq=22050;
	*loop=AL_FALSE;

	if (file)
	{
		    sfFile = (FILE *)fopen((const char *) file,"r");
			if(sfFile==NULL) exit(1);
		    numBytes = sizeof(WAVFileHdr_Struct);
			fread(&FileHdr,1,numBytes,sfFile);
			SwapWords((unsigned int *) &FileHdr.Size);
			FileHdr.Size=((FileHdr.Size+1)&~1)-4;
			while ((FileHdr.Size!=0))
			{
			    numBytes = sizeof(WAVChunkHdr_Struct);
				fread(&ChunkHdr,1,numBytes,sfFile);
			    if (numBytes != sizeof(WAVChunkHdr_Struct)) break;
			    SwapWords(&ChunkHdr.Size);
			    
				if ((ChunkHdr.Id[0] == 'f') && (ChunkHdr.Id[1] == 'm') && (ChunkHdr.Id[2] == 't') && (ChunkHdr.Id[3] == ' '))
				{
				    numBytes = sizeof(WAVFmtHdr_Struct);
					fread(&FmtHdr,1,numBytes,sfFile);
				    SwapBytes(&FmtHdr.Format);
					if (FmtHdr.Format==0x0001)
					{
					    SwapBytes(&FmtHdr.Channels);
					    SwapBytes(&FmtHdr.BitsPerSample);
					    SwapWords(&FmtHdr.SamplesPerSec);
					    SwapBytes(&FmtHdr.BlockAlign);
					    
						*format=(FmtHdr.Channels==1?
								(FmtHdr.BitsPerSample==8?AL_FORMAT_MONO8:AL_FORMAT_MONO16):
								(FmtHdr.BitsPerSample==8?AL_FORMAT_STEREO8:AL_FORMAT_STEREO16));
						*freq=FmtHdr.SamplesPerSec;
						fseek(sfFile,ChunkHdr.Size-sizeof(WAVFmtHdr_Struct),SEEK_CUR);
					} 
					else
					{
					    numBytes = sizeof(WAVFmtExHdr_Struct);
						fread(&FmtExHdr,1,numBytes,sfFile);
						fseek(sfFile, ChunkHdr.Size-sizeof(WAVFmtHdr_Struct)-sizeof(WAVFmtExHdr_Struct),SEEK_CUR);
					}
				}
				else if ((ChunkHdr.Id[0] == 'd') && (ChunkHdr.Id[1] == 'a') && (ChunkHdr.Id[2] == 't') && (ChunkHdr.Id[3] == 'a'))
				{
					if (FmtHdr.Format==0x0001)
					{
						*size=ChunkHdr.Size;
						if(*data == NULL){
							*data=malloc(ChunkHdr.Size);
						}
						else{
							realloc(*data,ChunkHdr.Size);
						}
						if (*data) 
						{
						    numBytes = ChunkHdr.Size;
							fread(*data,1,numBytes,sfFile);
						    if (FmtHdr.BitsPerSample == 16) 
						    {
						        for (i = 0; i < (numBytes / 2); i++)
						        {
						        	SwapBytes(&(*(unsigned short **)data)[i]);
						        }
						    }
						}
					}
					else if (FmtHdr.Format==0x0011)
					{
						//IMA ADPCM
					}
					else if (FmtHdr.Format==0x0055)
					{
						//MP3 WAVE
					}
				}
				else if ((ChunkHdr.Id[0] == 's') && (ChunkHdr.Id[1] == 'm') && (ChunkHdr.Id[2] == 'p') && (ChunkHdr.Id[3] == 'l'))
				{
				    numBytes = sizeof(WAVSmplHdr_Struct);
					fread(&SmplHdr,1,numBytes,sfFile);
					fseek(sfFile,ChunkHdr.Size-sizeof(WAVSmplHdr_Struct),SEEK_CUR);
				}
				else 
				{
					fseek(sfFile,ChunkHdr.Size,SEEK_CUR);
				}
				fseek(sfFile,ChunkHdr.Size&1,SEEK_CUR);
				FileHdr.Size-=(((ChunkHdr.Size+1)&~1)+8);
			    }
			fclose(sfFile);
		    }
}

ALUTAPI ALvoid ALUTAPIENTRY alutLoadWAVMemory(ALbyte *memory,ALenum *format,ALvoid **data,ALsizei *size,ALsizei *freq, ALboolean *loop)
{
	WAVChunkHdr_Struct ChunkHdr;
	WAVFmtExHdr_Struct FmtExHdr;
	WAVFileHdr_Struct FileHdr;
	WAVSmplHdr_Struct SmplHdr;
	WAVFmtHdr_Struct FmtHdr;
	int i;
	ALbyte *Stream;
	
	*format=AL_FORMAT_MONO16;
	*data=NULL;
	*size=0;
	*freq=22050;
	*loop=AL_FALSE;

	if (memory)
	{		
		Stream=memory;
		if (Stream)
		{
		    memcpy(&FileHdr,Stream,sizeof(WAVFileHdr_Struct));
		    Stream+=sizeof(WAVFileHdr_Struct);
			SwapWords((unsigned int *) &FileHdr.Size);
			FileHdr.Size=((FileHdr.Size+1)&~1)-4;
			while ((FileHdr.Size!=0)&&(memcpy(&ChunkHdr,Stream,sizeof(WAVChunkHdr_Struct))))
			{
				Stream+=sizeof(WAVChunkHdr_Struct);
			    SwapWords(&ChunkHdr.Size);
			    
				if ((ChunkHdr.Id[0] == 'f') && (ChunkHdr.Id[1] == 'm') && (ChunkHdr.Id[2] == 't') && (ChunkHdr.Id[3] == ' '))
				{
					memcpy(&FmtHdr,Stream,sizeof(WAVFmtHdr_Struct));
				    SwapBytes(&FmtHdr.Format);
					if (FmtHdr.Format==0x0001)
					{
					    SwapBytes(&FmtHdr.Channels);
					    SwapBytes(&FmtHdr.BitsPerSample);
					    SwapWords(&FmtHdr.SamplesPerSec);
					    SwapBytes(&FmtHdr.BlockAlign);
					    
						*format=(FmtHdr.Channels==1?
								(FmtHdr.BitsPerSample==8?AL_FORMAT_MONO8:AL_FORMAT_MONO16):
								(FmtHdr.BitsPerSample==8?AL_FORMAT_STEREO8:AL_FORMAT_STEREO16));
						*freq=FmtHdr.SamplesPerSec;
						Stream+=ChunkHdr.Size;
					} 
					else
					{
						memcpy(&FmtExHdr,Stream,sizeof(WAVFmtExHdr_Struct));
						Stream+=ChunkHdr.Size;
					}
				}
				else if ((ChunkHdr.Id[0] == 'd') && (ChunkHdr.Id[1] == 'a') && (ChunkHdr.Id[2] == 't') && (ChunkHdr.Id[3] == 'a'))
				{
					if (FmtHdr.Format==0x0001)
					{
						*size=ChunkHdr.Size;
						if(*data == NULL){
							*data=malloc(ChunkHdr.Size + 31);
						}
						else{
							realloc(*data,ChunkHdr.Size + 31);
						}
						if (*data) 
						{
							memcpy(*data,Stream,ChunkHdr.Size);
						    memset(((char *)*data)+ChunkHdr.Size,0,31);
							Stream+=ChunkHdr.Size;
						    if (FmtHdr.BitsPerSample == 16) 
						    {
						        for (i = 0; i < (ChunkHdr.Size / 2); i++)
						        {
						        	SwapBytes(&(*(unsigned short **)data)[i]);
						        }
						    }
						}
					}
					else if (FmtHdr.Format==0x0011)
					{
						//IMA ADPCM
					}
					else if (FmtHdr.Format==0x0055)
					{
						//MP3 WAVE
					}
				}
				else if ((ChunkHdr.Id[0] == 's') && (ChunkHdr.Id[1] == 'm') && (ChunkHdr.Id[2] == 'p') && (ChunkHdr.Id[3] == 'l'))
				{
				   	memcpy(&SmplHdr,Stream,sizeof(WAVSmplHdr_Struct));
					Stream+=ChunkHdr.Size;
				}
				else Stream+=ChunkHdr.Size;
				Stream+=ChunkHdr.Size&1;
				FileHdr.Size-=(((ChunkHdr.Size+1)&~1)+8);
			}
		}
	}
}

ALUTAPI ALvoid ALUTAPIENTRY alutUnloadWAV(ALenum format,ALvoid *data,ALsizei size,ALsizei freq)
{
    free(data);
} // alutUnloadWAV


#if HAVE_PRAGMA_EXPORT
#pragma export off
#endif

// end of ALut.c ...

