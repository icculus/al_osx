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

 function: window functions
 last mod: $Id: window.c,v 1.1 2003/10/12 23:51:40 root Exp $

 ********************************************************************/

#include <stdlib.h>
#include <math.h>
#include "os.h"
#include "misc.h"

float *_vorbis_window(int type, int left){
  float *ret=_ogg_calloc(left,sizeof(*ret));
  int i;

  switch(type){
  case 0:
    /* The 'vorbis window' (window 0) is sin(sin(x)*sin(x)*2pi) */
    {
    
      for(i=0;i<left;i++){
	float x=(i+.5f)/left*M_PI/2.;
	x=sin(x);
	x*=x;
	x*=M_PI/2.f;
	x=sin(x);
	ret[i]=x;
      }
    }
    break;
  default:
    _ogg_free(ret);
    return(NULL);
  }
  return(ret);
}


#if MACOSX
static void _vorbis_apply_window_vectorized(float *_d,float *window[2],
			  long *blocksizes, int lW,int W,int nW){
  lW=(W?lW:0);
  nW=(W?nW:0);

  {
    register long n=blocksizes[W];
    register long ln=blocksizes[lW];
    register long rn=blocksizes[nW];
    
    register long leftbegin=n/4-ln/4;
    register long leftend=leftbegin+ln/2;
    
    register long rightbegin=n/2+n/4-rn/4;
    register long rightend=rightbegin+rn/2;
    
    register float *d = _d;
    register float *win;
    register int extra = leftend % 4;
    register int i;

    register vector float vD;
    register vector float vWin;
    register vector float zero = (vector float) vec_splat_u32(0);
    register vector unsigned char permute = VPERMUTE4(3, 2, 1, 0);

    for(i=0;i<leftbegin;i++,d++)
      *d=0.f;

    d = _d + leftbegin;
    win = window[lW];
    leftend -= extra;
    for(i = leftbegin; i < leftend; i += 4)
    {
      vD = vec_ld(0, d);
      vWin = vec_ld(0, win);
      win += 4;
      vD = vec_madd(vD, vWin, zero);
      vec_st(vD, 0, d);
      d += 4;
    }

    leftend += extra;
    while (i < leftend)  // scalar for overflow.
    {
      *d *= *win;
      d++;
      win++;
      i++;
    }

    d = _d + rightbegin;
    win = window[nW] + ((rn>>1)-1);
    extra = rightend % 4;
    rightend -= extra;
    for(i = rightbegin; i < rightend; i += 4)
    {
      vD = vec_ld(0, d);
      vWin = vec_ld(0, win);
      win -= 4;
      vWin = vec_perm(vWin, vWin, permute);
      vD = vec_madd(vD, vWin, zero);
      vec_st(vD, 0, d);
      d += 4;
    }

    rightend += extra;
    while (i < rightend)  // scalar for overflow.
    {
      *d *= *win;
      d++;
      win++;
      i++;
    }

    d = _d + i;
    for(;i<n;i++,d++)
      *d=0.f;
  }
}

#else  /* no vectorized code for this platform. */

STIN void _vorbis_apply_window_vectorized(float *d,float *window[2],
			  long *blocksizes, int lW,int W,int nW){
    /* Landing in here means something is broken. */
}
#endif


void _vorbis_apply_window(float *d,float *window[2],long *blocksizes,
			  int lW,int W,int nW){

  if (_vorbis_has_vector_unit()) {
    _vorbis_apply_window_vectorized(d, window, blocksizes, lW, W, nW);
    return;
  }

  lW=(W?lW:0);
  nW=(W?nW:0);

  {
    long n=blocksizes[W];
    long ln=blocksizes[lW];
    long rn=blocksizes[nW];
    
    long leftbegin=n/4-ln/4;
    long leftend=leftbegin+ln/2;
    
    long rightbegin=n/2+n/4-rn/4;
    long rightend=rightbegin+rn/2;
    
    int i,p;

    for(i=0;i<leftbegin;i++)
      d[i]=0.f;

    for(p=0;i<leftend;i++,p++)
      d[i]*=window[lW][p];

    for(i=rightbegin,p=rn/2-1;i<rightend;i++,p--)
      d[i]*=window[nW][p];
    
    for(;i<n;i++)
      d[i]=0.f;
  }
}
