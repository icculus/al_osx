/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2002             *
 * by the XIPHOPHORUS Company http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: miscellaneous prototypes
 last mod: $Id: misc.h,v 1.1 2003/10/12 23:51:40 root Exp $

 ********************************************************************/

#ifndef _V_RANDOM_H_
#define _V_RANDOM_H_
#include "vorbis/codec.h"

#if MACOSX
   extern int __alHasEnabledVectorUnit;
#  define _vorbis_has_vector_unit() (__alHasEnabledVectorUnit)
#  define UNALIGNED_PTR(x) (((size_t) (x)) & 0x0000000F)
#  define VPERMUTE4(a,b,c,d) (vector unsigned char) \
                               ( (a*4)+0, (a*4)+1, (a*4)+2, (a*4)+3, \
                                 (b*4)+0, (b*4)+1, (b*4)+2, (b*4)+3, \
                                 (c*4)+0, (c*4)+1, (c*4)+2, (c*4)+3, \
                                 (d*4)+0, (d*4)+1, (d*4)+2, (d*4)+3 )
#else
   /* Say everything is unaligned so compiler can optimize out the branches. */
#  define UNALIGNED_PTR(x) (1)
   /* Always false until SSE or whatnot support shows up. */
#  define _vorbis_has_vector_unit() (0)
#endif

extern int analysis_noisy;

extern void *_vorbis_block_alloc(vorbis_block *vb,long bytes);
extern void _vorbis_block_ripcord(vorbis_block *vb);
extern void _analysis_output(char *base,int i,float *v,int n,int bark,int dB,
			     ogg_int64_t off);

#ifdef DEBUG_MALLOC

#define _VDBG_GRAPHFILE "malloc.m"
extern void *_VDBG_malloc(void *ptr,long bytes,char *file,long line); 
extern void _VDBG_free(void *ptr,char *file,long line); 

#ifndef MISC_C 
#undef _ogg_malloc
#undef _ogg_calloc
#undef _ogg_realloc
#undef _ogg_free

#define _ogg_malloc(x) _VDBG_malloc(NULL,(x),__FILE__,__LINE__)
#define _ogg_calloc(x,y) _VDBG_malloc(NULL,(x)*(y),__FILE__,__LINE__)
#define _ogg_realloc(x,y) _VDBG_malloc((x),(y),__FILE__,__LINE__)
#define _ogg_free(x) _VDBG_free((x),__FILE__,__LINE__)
#endif
#endif

#endif




