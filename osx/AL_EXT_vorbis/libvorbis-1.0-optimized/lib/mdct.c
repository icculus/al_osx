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

 function: normalized modified discrete cosine transform
           power of two length transform only [64 <= n ]
 last mod: $Id: mdct.c,v 1.1 2003/10/12 23:51:40 root Exp $

 Original algorithm adapted long ago from _The use of multirate filter
 banks for coding of high quality digital audio_, by T. Sporer,
 K. Brandenburg and B. Edler, collection of the European Signal
 Processing Conference (EUSIPCO), Amsterdam, June 1992, Vol.1, pp
 211-214

 The below code implements an algorithm that no longer looks much like
 that presented in the paper, but the basic structure remains if you
 dig deep enough to see it.

 This module DOES NOT INCLUDE code to generate/apply the window
 function.  Everybody has their own weird favorite including me... I
 happen to like the properties of y=sin(2PI*sin^2(x)), but others may
 vehemently disagree.

 ********************************************************************/

/* this can also be run as an integer transform by uncommenting a
   define in mdct.h; the integerization is a first pass and although
   it's likely stable for Vorbis, the dynamic range is constrained and
   roundoff isn't done (so it's noisy).  Consider it functional, but
   only a starting point.  There's no point on a machine with an FPU */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "vorbis/codec.h"
#include "mdct.h"
#include "os.h"
#include "misc.h"

STIN void mdct_butterflies_vectorized(mdct_lookup *init,DATA_TYPE *x,int points);

#ifdef MACOSX
STIN void mdct_butterfly_first_vectorized(DATA_TYPE *_T,DATA_TYPE *x,int points){
    register float *T = _T;
    register float *x1        = x          + points      - 8;
    register float *x2        = x          + (points>>1) - 8;

    register vector float x1a;
    register vector float x1b;
    register vector float x2a;
    register vector float x2b;
    register vector float vTa;
    register vector float vTb;
    register vector float vTc;
    register vector float vTd;

    register vector float vR;
    register vector float vMinus;
    register vector float vTmp;
    register vector float zero = (vector float) vec_splat_u32(0);

    register vector unsigned char permute1 = VPERMUTE4(0, 1, 0, 1);
    register vector unsigned char permute2; //= VPERMUTE4(2, 3, 2, 3);
    register vector unsigned char permute3 = VPERMUTE4(0, 1, 1, 0);
    register vector unsigned char permute4 = VPERMUTE4(2, 0, 6, 4);

    permute2 = vec_add(permute1, vec_splat_u8(8));

    do{
        x1a = vec_ld(0x00, x1);
        x1b = vec_ld(0x10, x1);
        x2a = vec_ld(0x00, x2);
        x2b = vec_ld(0x10, x2);

        vTa = vec_ld(0x00, T);
        vTb = vec_ld(0x10, T);
        vTc = vec_ld(0x20, T);
        vTd = vec_ld(0x30, T);

        vMinus = vec_sub(x1b, x2b);
        x1b = vec_add(x1b, x2b);
        vR = vec_perm(vMinus, vMinus, permute2);  // r0 r1 r0 r1
//               r0      = x1[6]      -  x2[6];
//	       r1      = x1[7]      -  x2[7];
//	       x1[6]  += x2[6];
//	       x1[7]  += x2[7];
        vTa = vec_perm(vTa, vTa, permute3);       // t0 t1 t1 t0
        vTa = vec_madd(vR, vTa, zero);   // r0t0 r1t1 r0t1 r1t0
        vR = vec_sld(vTa, vTa, 4);       // r1t1 r0t1 r1t0 r0t0
        vTmp = vec_add(vR, vTa);         // r1t1+r0t0 x x x
        vR = vec_sub(vR, vTa);           // x x r1t0-r0t1 x
        x2b = vec_sld(vR, vTmp, 8);      // x2[7] x x2[6] x
        
//	       x2[6]   = MULT_NORM(r1 * T[1]  +  r0 * T[0]);
//	       x2[7]   = MULT_NORM(r1 * T[0]  -  r0 * T[1]);
	       
        vR = vec_perm(vMinus, vMinus, permute1);  // r0 r1 r0 r1
//	       r0      = x1[4]      -  x2[4];
//	       r1      = x1[5]      -  x2[5];
//	       x1[4]  += x2[4];
//	       x1[5]  += x2[5];

        vTb = vec_perm(vTb, vTb, permute3);
        vTb = vec_madd(vR, vTb, zero);   // r0t0 r1t1 r0t1 r1t0
        vR = vec_sld(vTb, vTb, 4);       // r1t1 r0t1 r1t0 r0t0
        vTmp = vec_add(vR, vTb);         // r1t1+r0t0 x x x
        vR = vec_sub(vR, vTb);           // x x r1t0-r0t1 x
        vTmp = vec_sld(vR, vTmp, 8);     // x2[5] x x2[4] x
        x2b = vec_perm(vTmp, x2b, permute4);

//	       x2[4]   = MULT_NORM(r1 * T[5]  +  r0 * T[4]);
//	       x2[5]   = MULT_NORM(r1 * T[4]  -  r0 * T[5]);

        vMinus = vec_sub(x1a, x2a);
        x1a = vec_add(x1a, x2a);
        vR = vec_perm(vMinus, vMinus, permute2);  // r0 r1 r0 r1
//	       r0      = x1[2]      -  x2[2];
//	       r1      = x1[3]      -  x2[3];
//	       x1[2]  += x2[2];
//	       x1[3]  += x2[3];

        vTc = vec_perm(vTc, vTc, permute3);
        vTc = vec_madd(vR, vTc, zero);   // r0t0 r1t1 r0t1 r1t0
        vR = vec_sld(vTc, vTc, 4);       // r1t1 r0t1 r1t0 r0t0
        vTmp = vec_add(vR, vTc);         // r1t1+r0t0 x x x
        vR = vec_sub(vR, vTc);           // x x r1t0-r0t1 x
        x2a = vec_sld(vR, vTmp, 8);     // x2[3] x x2[2] x

//	       x2[2]   = MULT_NORM(r1 * T[9]  +  r0 * T[8]);
//	       x2[3]   = MULT_NORM(r1 * T[8]  -  r0 * T[9]);
	       
        vR = vec_perm(vMinus, vMinus, permute1);  // r0 r1 r0 r1
//	       r0      = x1[0]      -  x2[0];
//	       r1      = x1[1]      -  x2[1];
//	       x1[0]  += x2[0];
//	       x1[1]  += x2[1];

        vTd = vec_perm(vTd, vTd, permute3);
        vTd = vec_madd(vR, vTd, zero);   // r0t0 r1t1 r0t1 r1t0
        vR = vec_sld(vTd, vTd, 4);       // r1t1 r0t1 r1t0 r0t0
        vTmp = vec_add(vR, vTd);         // r1t1+r0t0 x x x
        vR = vec_sub(vR, vTd);           // x x r1t0-r0t1 x
        vTmp = vec_sld(vR, vTmp, 8);     // x2[1] x x2[0] x
        x2a = vec_perm(vTmp, x2a, permute4);
//	       x2[0]   = MULT_NORM(r1 * T[13] +  r0 * T[12]);
//	       x2[1]   = MULT_NORM(r1 * T[12] -  r0 * T[13]);
	       
        vec_st(x1a, 0x00, x1);
        vec_st(x1b, 0x10, x1);
        vec_st(x2a, 0x00, x2);
        vec_st(x2b, 0x10, x2);

        x1 -= 8;
        x2 -= 8;
        T += 16;
    }while(x2>=x);
}

static void mdct_butterfly_8_vectorized(DATA_TYPE *_x){
    register float *x = _x;
    register vector float x0to3;
    register vector float x4to7;

    register vector float vPlus;
    register vector float vMinus;
    register vector float vTmp;
    register vector float vTmp2;
    register vector float vTmp3;

    register vector unsigned char permute1 = VPERMUTE4(4, 0, 0, 0);
    register vector unsigned char permute2 = VPERMUTE4(0, 4, 2, 6);

    x0to3   = vec_ld(0x00, x);
    x4to7   = vec_ld(0x10, x);

    vMinus = vec_sub(x4to7, x0to3);
    vPlus = vec_add(x4to7, x0to3);

    vTmp = vec_sld(vPlus, vPlus, 8);
    vTmp2 = vec_add(vTmp, vPlus); // x6 x x x
    vTmp3 = vec_sub(vTmp, vPlus); // x4 x x x
    x4to7 = vec_perm(vTmp2, vTmp3, permute1);  // x4 x x6 x
//  r0   = x[6] + x[2];
//  r1   = x[6] - x[2];
//  r2   = x[4] + x[0];
//  r3   = x[4] - x[0];
//	   x[6] = r0   + r2;
//	   x[4] = r0   - r2;
	   
    vTmp = vec_sld(vMinus, vMinus, 4);
    vTmp2 = vec_add(vTmp, vMinus); // x x0 x x
    vTmp3 = vec_sub(vTmp, vMinus); // x x2 x x
    vTmp2 = vec_sld(vTmp2, vTmp2, 4);
    vTmp3 = vec_sld(vTmp3, vTmp3, 4);
    x0to3 = vec_perm(vTmp3, vTmp2, permute1);  // x0 x x2 x
//	   r0   = x[5] - x[1];
//	   r2   = x[7] - x[3];
//	   x[0] = r1   + r0;
//	   x[2] = r1   - r0;

    vTmp = vec_sld(vMinus, vMinus, 12);
    vTmp2 = vec_add(vTmp, vMinus); // x3 x x x
    vTmp3 = vec_sub(vTmp, vMinus); // x1 x x x
    vTmp = vec_perm(vTmp2, vTmp3, permute1);   // x1 x x3 x
    x0to3 = vec_perm(x0to3, vTmp, permute2);
//	   r0   = x[5] + x[1];
//	   r1   = x[7] + x[3];
//	   x[3] = r2   + r3;
//	   x[1] = r2   - r3;

    vTmp = vec_sld(vPlus, vPlus, 8);
    vTmp2 = vec_add(vTmp, vPlus); // x x7 x x
    vTmp3 = vec_sub(vTmp, vPlus); // x x5 x x
    vTmp2 = vec_sld(vTmp2, vTmp2, 4);
    vTmp3 = vec_sld(vTmp3, vTmp3, 4);
    vTmp = vec_perm(vTmp2, vTmp3, permute1);   // x5 x x7 x
    x4to7 = vec_perm(x4to7, vTmp, permute2);
    
//	   x[7] = r1   + r0;
//	   x[5] = r1   - r0;
	   
    vec_st(x0to3,   0x00, x);
    vec_st(x4to7,   0x10, x);
}

static void mdct_butterfly_16_vectorized(DATA_TYPE *_x){
    register float *x = _x;
    register vector float x0to3;
    register vector float x4to7;
    register vector float x8to11;
    register vector float x12to15;

    register vector float vR;
    register vector float vMinus;
    register vector float vTmp;
    register vector float vTmp2;
    register vector float vTmp3;

    register vector float zero = (vector float) vec_splat_u32(0);

    register vector float vPi2 = (vector float) (cPI2_8,cPI2_8,cPI2_8,cPI2_8);

    register vector unsigned char permute1 = VPERMUTE4(0, 1, 0, 1);
    register vector unsigned char permute2 = VPERMUTE4(3, 6, 3, 6); // !!! FIXME, add 1 to permute1 and shift left 1.
    register vector unsigned char permute3 = VPERMUTE4(0, 3, 4, 5);
    register vector unsigned char permute4 = VPERMUTE4(0, 2, 4, 5);
//    register vector unsigned char permute4 = VPERMUTE4(0, 4, 1, 5);

    x0to3   = vec_ld(0x00, x);
    x4to7   = vec_ld(0x10, x);
    x8to11  = vec_ld(0x20, x);
    x12to15 = vec_ld(0x30, x);

    vMinus = vec_sub(x0to3, x8to11);
    vR = vec_perm(vMinus, vMinus, permute1);  // r1 r0 r1 r0
//           r0     = x[1]  - x[9];
//           r1     = x[0]  - x[8];
//           x[8]  += x[0];
//           x[9]  += x[1];

    vTmp = vec_sld(vR, vR, 4);     // r0 r1 r0 r1
    vTmp2 = vec_add(vR, vTmp);     // r0+r1 r0+r1 r0+r1 r0+r1
    vTmp3 = vec_sub(vR, vTmp);     // r1-r0 r0-r1 r1-r0 r0-r1
    vTmp2 = vec_madd(vTmp2, vPi2, zero);
    vTmp3 = vec_madd(vTmp3, vPi2, zero);
    vTmp = vec_sld(vTmp2, vTmp3, 8);  // (r0+r1)p2 x x (r0-r1)p2
//           x[0]   = MULT_NORM((r0   + r1) * cPI2_8);
//           x[1]   = MULT_NORM((r0   - r1) * cPI2_8);

    vR = vec_sub(x8to11, x0to3);
    vR = vec_perm(vMinus, vR, permute2);  // r0 r1 r0 r1
    x8to11 = vec_add(x0to3, x8to11);
    x0to3 = vec_perm(vTmp, vR, permute3);
//           r0     = x[3]  - x[11];
//           r1     = x[10] - x[2];
//           x[10] += x[2];
//           x[11] += x[3];
//           x[2]   = r0;
//           x[3]   = r1;

    vMinus = vec_sub(x12to15, x4to7);
    vR = vec_perm(vMinus, vMinus, permute1);  // r0 r1 r0 r1
    x12to15 = vec_add(x12to15, x4to7);
//           r0     = x[12] - x[4];
//           r1     = x[13] - x[5];
//           x[12] += x[4];
//           x[13] += x[5];


    vTmp = vec_sld(vR, vR, 4);   // r1 r0 r1 r0
    vTmp2 = vec_add(vR, vTmp);   // r1+r0 r1+r0 r1+r0 r1+r0
    vTmp3 = vec_sub(vR, vTmp);   // r0-r1 r1-r0 r0-r1 r1-r0
    vTmp2 = vec_madd(vTmp2, vPi2, zero);
    vTmp3 = vec_madd(vTmp3, vPi2, zero);
    x4to7 = vec_sld(vTmp3, vTmp2, 8);  // x4 x x5 x   misordered!
//           x[4]   = MULT_NORM((r0   - r1) * cPI2_8);
//           x[5]   = MULT_NORM((r0   + r1) * cPI2_8);

    vR = vec_perm(vMinus, vMinus, vec_add(permute1, vec_splat_u8(8)));
//           r0     = x[14] - x[6];
//           r1     = x[15] - x[7];
//           x[14] += x[6];
//           x[15] += x[7];

    x4to7 = vec_perm(x4to7, vR, permute4);
//           x[6]  = r0;
//           x[7]  = r1;

    vec_st(x0to3,   0x00, x);
    vec_st(x4to7,   0x10, x);
    vec_st(x8to11,  0x20, x);
    vec_st(x12to15, 0x30, x);

	   mdct_butterfly_8_vectorized(x);
	   mdct_butterfly_8_vectorized(x+8);
}

static void mdct_butterfly_32_vectorized(DATA_TYPE *_x){
    register float *x = _x;
    register vector float x0to3;
    register vector float x4to7;
    register vector float x8to11;
    register vector float x12to15;
    register vector float x16to19;
    register vector float x20to23;
    register vector float x24to27;
    register vector float x28to31;
    register vector float vPlus;
    register vector float vMinus;
    register vector float vR;
    register vector float vTmp;
    register vector float vTmp2;
    register vector float vTmp3;

    register vector float zero = (vector float) vec_splat_u32(0);

    register vector float vPi1 = (vector float) (cPI3_8,cPI1_8,cPI1_8,cPI3_8);
    register vector float vPi2 = (vector float) (cPI2_8,cPI2_8,cPI2_8,cPI2_8);

    register vector unsigned char permute1 = VPERMUTE4(0, 1, 6, 7);
    register vector unsigned char permute2 = VPERMUTE4(0, 1, 0, 1);
    register vector unsigned char permute3 = VPERMUTE4(5, 7, 2, 3);
    register vector unsigned char permute4 = VPERMUTE4(7, 4, 1, 3);
    register vector unsigned char permute5 = VPERMUTE4(0, 0, 7, 2);
    register vector unsigned char permute6 = VPERMUTE4(2, 0, 6, 7);
    register vector unsigned char permute7; // = VPERMUTE4(2, 3, 2, 3);
    register vector unsigned char permute8 = VPERMUTE4(6, 4, 1, 3);
    register vector unsigned char permute9 = VPERMUTE4(3, 6, 3, 6); // !!! FIXME, add 1 to permute1 and shift left 1.
    register vector unsigned char permute10 = VPERMUTE4(0, 3, 4, 5);
    register vector unsigned char permute11 = VPERMUTE4(0, 2, 4, 5);
    register vector unsigned char permute12 = VPERMUTE4(4, 0, 0, 0);
    register vector unsigned char permute13 = VPERMUTE4(0, 4, 2, 6);

    x0to3   = vec_ld(0x00, x);
    x4to7   = vec_ld(0x10, x);
    x8to11  = vec_ld(0x20, x);
    x12to15 = vec_ld(0x30, x);
    x16to19 = vec_ld(0x40, x);
    x20to23 = vec_ld(0x50, x);
    x24to27 = vec_ld(0x60, x);
    x28to31 = vec_ld(0x70, x);

    permute7 = vec_add(permute2, vec_splat_u8(8));

    vR = vec_sub(x28to31, x12to15);
    x28to31 = vec_add(x28to31, x12to15);
    x12to15 = vec_perm(x12to15, vR, permute1);  // x x y y
//  r0     = x[30] - x[14];
//  r1     = x[31] - x[15];
//
//     x[30] +=         x[14];
//     x[31] +=         x[15];
//     x[14]  =         r0;
//     x[15]  =         r1;

    vR = vec_perm(vR, vR, permute2);  // r0 r1 r0 r1
//  r0     = x[28] - x[12];
//	r1     = x[29] - x[13];
//     x[28] +=         x[12];
//     x[29] +=         x[13];

    vR = vec_madd(vR, vPi1, zero);     // r0p3 r1p1 r0p1 r1p3
    vTmp = vec_sld(vR, vR, 4);   // r1p1 r0p1 r1p3 r0p3

    vTmp2 = vec_sub(vR, vTmp);       // x x r0p1-r1p3 x
    vTmp3 = vec_add(vR, vTmp);       // r0p3+r1p1 x x x
    vTmp = vec_sld(vTmp2, vTmp3, 4); // x r0p1-r1p3 x r0p3+r1p1
    x12to15 = vec_perm(x12to15, vTmp, permute3);

//     x[12]  = MULT_NORM( r0 * cPI1_8  -  r1 * cPI3_8 );
//     x[13]  = MULT_NORM( r0 * cPI3_8  +  r1 * cPI1_8 );

    vMinus = vec_sub(x24to27, x8to11);
    x24to27 = vec_add(x24to27, x8to11);

// r0     = x[26] - x[10];
// r1     = x[27] - x[11];
//     x[26] +=         x[10];
//     x[27] +=         x[11];

    vR = vec_perm(vMinus, vMinus, permute7);  // r0 r1 r0 r1
    vTmp = vec_sld(vR, vR, 4);                // r1 r0 r1 r0
    vTmp2 = vec_sub(vR, vTmp);                // x x r0-r1 x
    vTmp3 = vec_add(vR, vTmp);                // r0+r1 x x x
    vTmp = vec_sld(vTmp2, vTmp3, 4);          // x r0-r1 x r0+r1
    x8to11 = vec_madd(vTmp, vPi2, zero); // in wrong order.
//     x[10]  = MULT_NORM(( r0  - r1 ) * cPI2_8);
//     x[11]  = MULT_NORM(( r0  + r1 ) * cPI2_8);

    vR = vec_perm(vMinus, vMinus, permute2);
// r0     = x[24] - x[8];
// r1     = x[25] - x[9];
//     x[24] += x[8];
//     x[25] += x[9];

    vR = vec_madd(vR, vec_sld(vPi1, vPi1, 12), zero); // r0p3 r1p3 r0p1 r1p1
    vTmp = vec_sld(vR, vR, 4);                        // r1p3 r0p1 r1p1 r0p3
    vTmp2 = vec_add(vR, vTmp);  // x r1p3+r0p1 x x
    vTmp = vec_sld(vR, vR, 12);                       // r1p1 r0p3 r1p3 r0p1
    vTmp3 = vec_sub(vR, vTmp);  // r0p3-r1p1 x x x
    vTmp = vec_sld(vTmp2, vTmp3, 4);  // r1p3+r0p1 x x r0p3-r1p1
    x8to11 = vec_perm(x8to11, vTmp, permute4);
//	   x[8]   = MULT_NORM( r0 * cPI3_8  -  r1 * cPI1_8 );
//	   x[9]   = MULT_NORM( r1 * cPI3_8  +  r0 * cPI1_8 );

    vMinus = vec_sub(x20to23, x4to7);  // x x r0 x
    vR = vec_sub(x4to7, x20to23);      // x x x r1
    x20to23 = vec_add(x20to23, x4to7);
// r0     = x[22] - x[6];
// r1     = x[7]  - x[23];
//     x[22] += x[6];
//     x[23] += x[7];

    x4to7 = vec_perm(vMinus, vR, permute5);  // x x r1 r0
//     x[6]   = r1;
//     x[7]   = r0;

    vR = vec_perm(vR, vR, permute2); // r0 r1 r0 r1
// r0     = x[4]  - x[20];
// r1     = x[5]  - x[21];
//     x[20] += x[4];
//     x[21] += x[5];

    vR = vec_madd(vR, vPi1, zero);    // r0p3 r1p1 r0p1 r1p3
    vTmp = vec_sld(vR, vR, 4);        // r1p1 r0p1 r1p3 r0p3
    vTmp2 = vec_sub(vTmp, vR);        // x x r1p3-r0p1 x
    vTmp3 = vec_add(vTmp, vR);        // r1p1+r0p3 x x x
    vTmp = vec_sld(vTmp2, vTmp3, 8);  // r1p3-r0p1 x r1p1+r0p3 x
    x4to7 = vec_perm(vTmp, x4to7, permute6); // x x X X
//     x[4]   = MULT_NORM( r1 * cPI1_8  +  r0 * cPI3_8 );
//     x[5]   = MULT_NORM( r1 * cPI3_8  -  r0 * cPI1_8 );

    vMinus = vec_sub(x0to3, x16to19);
    x16to19 = vec_add(x0to3, x16to19);
    vR = vec_perm(vMinus, vMinus, permute7);  // r0 r1 r0 r1
// r0     = x[2]  - x[18];
// r1     = x[3]  - x[19];
//     x[18] += x[2];
//     x[19] += x[3];

    vTmp = vec_sld(vR, vR, 4);  // r1 r0 r1 r0
    vTmp2 = vec_add(vTmp, vR);  // r1+r0 r1+r0 r1+r0 r1+r0
    vTmp3 = vec_sub(vTmp, vR);  // r1-r0 r0-r1 r1-r0 r0-r1
    vTmp2 = vec_madd(vTmp2, vPi2, zero);
    vTmp3 = vec_madd(vTmp3, vPi2, zero);
    x0to3 = vec_sld(vTmp2, vTmp3, 4); // x x2 x x3
//     x[2]   = MULT_NORM(( r1  + r0 ) * cPI2_8);
//     x[3]   = MULT_NORM(( r1  - r0 ) * cPI2_8);

    vR = vec_perm(vMinus, vMinus, vec_sld(permute2, permute2, 4)); // r1 r0 r1 r0
// r0     = x[0]  - x[16];
// r1     = x[1]  - x[17];
//     x[16] += x[0];
//     x[17] += x[1];

    vR = vec_madd(vR, vPi1, zero); // r1p3 r0p1 r1p1 r0p3
    vTmp = vec_sld(vR, vR, 4);     // r0p1 r1p1 r0p3 r1p3
    vTmp2 = vec_sub(vR, vTmp);     // x x r1p1-r0p3 x
    vTmp3 = vec_add(vTmp, vR);     // r1p3+r0p1 x x x
    vTmp = vec_sld(vTmp2, vTmp3, 8);  // r1p1-r0p3 x r1p3+r0p1 x
    x0to3 = vec_perm(x0to3, vTmp, permute8);
//     x[0]   = MULT_NORM( r1 * cPI3_8  +  r0 * cPI1_8 );
//     x[1]   = MULT_NORM( r1 * cPI1_8  -  r0 * cPI3_8 );

///  ----  butterfly16 (x)
    vMinus = vec_sub(x0to3, x8to11);
    vR = vec_perm(vMinus, vMinus, permute2);  // r1 r0 r1 r0
//           r0     = x[1]  - x[9];
//           r1     = x[0]  - x[8];
//           x[8]  += x[0];
//           x[9]  += x[1];

    vTmp = vec_sld(vR, vR, 4);     // r0 r1 r0 r1
    vTmp2 = vec_add(vR, vTmp);     // r0+r1 r0+r1 r0+r1 r0+r1
    vTmp3 = vec_sub(vR, vTmp);     // r1-r0 r0-r1 r1-r0 r0-r1
    vTmp2 = vec_madd(vTmp2, vPi2, zero);
    vTmp3 = vec_madd(vTmp3, vPi2, zero);
    vTmp = vec_sld(vTmp2, vTmp3, 8);  // (r0+r1)p2 x x (r0-r1)p2
//           x[0]   = MULT_NORM((r0   + r1) * cPI2_8);
//           x[1]   = MULT_NORM((r0   - r1) * cPI2_8);

    vR = vec_sub(x8to11, x0to3);
    vR = vec_perm(vMinus, vR, permute9);  // r0 r1 r0 r1
    x8to11 = vec_add(x0to3, x8to11);
    x0to3 = vec_perm(vTmp, vR, permute10);
//           r0     = x[3]  - x[11];
//           r1     = x[10] - x[2];
//           x[10] += x[2];
//           x[11] += x[3];
//           x[2]   = r0;
//           x[3]   = r1;

    vMinus = vec_sub(x12to15, x4to7);
    vR = vec_perm(vMinus, vMinus, permute2);  // r0 r1 r0 r1
    x12to15 = vec_add(x12to15, x4to7);
//           r0     = x[12] - x[4];
//           r1     = x[13] - x[5];
//           x[12] += x[4];
//           x[13] += x[5];


    vTmp = vec_sld(vR, vR, 4);   // r1 r0 r1 r0
    vTmp2 = vec_add(vR, vTmp);   // r1+r0 r1+r0 r1+r0 r1+r0
    vTmp3 = vec_sub(vR, vTmp);   // r0-r1 r1-r0 r0-r1 r1-r0
    vTmp2 = vec_madd(vTmp2, vPi2, zero);
    vTmp3 = vec_madd(vTmp3, vPi2, zero);
    x4to7 = vec_sld(vTmp3, vTmp2, 8);  // x4 x x5 x   misordered!
//           x[4]   = MULT_NORM((r0   - r1) * cPI2_8);
//           x[5]   = MULT_NORM((r0   + r1) * cPI2_8);

    vR = vec_perm(vMinus, vMinus, permute7);
//           r0     = x[14] - x[6];
//           r1     = x[15] - x[7];
//           x[14] += x[6];
//           x[15] += x[7];

    x4to7 = vec_perm(x4to7, vR, permute11);
//           x[6]  = r0;
//           x[7]  = r1;

///  ----  butterfly8 (x)
    vMinus = vec_sub(x4to7, x0to3);
    vPlus = vec_add(x4to7, x0to3);

    vTmp = vec_sld(vPlus, vPlus, 8);
    vTmp2 = vec_add(vTmp, vPlus); // x6 x x x
    vTmp3 = vec_sub(vTmp, vPlus); // x4 x x x
    x4to7 = vec_perm(vTmp2, vTmp3, permute12);  // x4 x x6 x
//  r0   = x[6] + x[2];
//  r1   = x[6] - x[2];
//  r2   = x[4] + x[0];
//  r3   = x[4] - x[0];
//	   x[6] = r0   + r2;
//	   x[4] = r0   - r2;
	   
    vTmp = vec_sld(vMinus, vMinus, 4);
    vTmp2 = vec_add(vTmp, vMinus); // x x0 x x
    vTmp3 = vec_sub(vTmp, vMinus); // x x2 x x
    vTmp2 = vec_sld(vTmp2, vTmp2, 4);
    vTmp3 = vec_sld(vTmp3, vTmp3, 4);
    x0to3 = vec_perm(vTmp3, vTmp2, permute12);  // x0 x x2 x
//	   r0   = x[5] - x[1];
//	   r2   = x[7] - x[3];
//	   x[0] = r1   + r0;
//	   x[2] = r1   - r0;

    vTmp = vec_sld(vMinus, vMinus, 12);
    vTmp2 = vec_add(vTmp, vMinus); // x3 x x x
    vTmp3 = vec_sub(vTmp, vMinus); // x1 x x x
    vTmp = vec_perm(vTmp2, vTmp3, permute12);   // x1 x x3 x
    x0to3 = vec_perm(x0to3, vTmp, permute13);
//	   r0   = x[5] + x[1];
//	   r1   = x[7] + x[3];
//	   x[3] = r2   + r3;
//	   x[1] = r2   - r3;

    vTmp = vec_sld(vPlus, vPlus, 8);
    vTmp2 = vec_add(vTmp, vPlus); // x x7 x x
    vTmp3 = vec_sub(vTmp, vPlus); // x x5 x x
    vTmp2 = vec_sld(vTmp2, vTmp2, 4);
    vTmp3 = vec_sld(vTmp3, vTmp3, 4);
    vTmp = vec_perm(vTmp2, vTmp3, permute12);   // x5 x x7 x
    x4to7 = vec_perm(x4to7, vTmp, permute13);
    
//	   x[7] = r1   + r0;
//	   x[5] = r1   - r0;

///  ----  butterfly8 (x+8)
    vMinus = vec_sub(x12to15, x8to11);
    vPlus = vec_add(x12to15, x8to11);

    vTmp = vec_sld(vPlus, vPlus, 8);
    vTmp2 = vec_add(vTmp, vPlus); // x6 x x x
    vTmp3 = vec_sub(vTmp, vPlus); // x4 x x x
    x12to15 = vec_perm(vTmp2, vTmp3, permute12);  // x4 x x6 x
//  r0   = x[6] + x[2];
//  r1   = x[6] - x[2];
//  r2   = x[4] + x[0];
//  r3   = x[4] - x[0];
//	   x[6] = r0   + r2;
//	   x[4] = r0   - r2;
	   
    vTmp = vec_sld(vMinus, vMinus, 4);
    vTmp2 = vec_add(vTmp, vMinus); // x x0 x x
    vTmp3 = vec_sub(vTmp, vMinus); // x x2 x x
    vTmp2 = vec_sld(vTmp2, vTmp2, 4);
    vTmp3 = vec_sld(vTmp3, vTmp3, 4);
    x8to11 = vec_perm(vTmp3, vTmp2, permute12);  // x0 x x2 x
//	   r0   = x[5] - x[1];
//	   r2   = x[7] - x[3];
//	   x[0] = r1   + r0;
//	   x[2] = r1   - r0;

    vTmp = vec_sld(vMinus, vMinus, 12);
    vTmp2 = vec_add(vTmp, vMinus); // x3 x x x
    vTmp3 = vec_sub(vTmp, vMinus); // x1 x x x
    vTmp = vec_perm(vTmp2, vTmp3, permute12);   // x1 x x3 x
    x8to11 = vec_perm(x8to11, vTmp, permute13);
//	   r0   = x[5] + x[1];
//	   r1   = x[7] + x[3];
//	   x[3] = r2   + r3;
//	   x[1] = r2   - r3;

    vTmp = vec_sld(vPlus, vPlus, 8);
    vTmp2 = vec_add(vTmp, vPlus); // x x7 x x
    vTmp3 = vec_sub(vTmp, vPlus); // x x5 x x
    vTmp2 = vec_sld(vTmp2, vTmp2, 4);
    vTmp3 = vec_sld(vTmp3, vTmp3, 4);
    vTmp = vec_perm(vTmp2, vTmp3, permute12);   // x5 x x7 x
    x12to15 = vec_perm(x12to15, vTmp, permute13);
    
//	   x[7] = r1   + r0;
//	   x[5] = r1   - r0;

///  ----  butterfly16 (x+16)
    vMinus = vec_sub(x16to19, x24to27);
    vR = vec_perm(vMinus, vMinus, permute2);  // r1 r0 r1 r0
//           r0     = x[1]  - x[9];
//           r1     = x[0]  - x[8];
//           x[8]  += x[0];
//           x[9]  += x[1];

    vTmp = vec_sld(vR, vR, 4);     // r0 r1 r0 r1
    vTmp2 = vec_add(vR, vTmp);     // r0+r1 r0+r1 r0+r1 r0+r1
    vTmp3 = vec_sub(vR, vTmp);     // r1-r0 r0-r1 r1-r0 r0-r1
    vTmp2 = vec_madd(vTmp2, vPi2, zero);
    vTmp3 = vec_madd(vTmp3, vPi2, zero);
    vTmp = vec_sld(vTmp2, vTmp3, 8);  // (r0+r1)p2 x x (r0-r1)p2
//           x[0]   = MULT_NORM((r0   + r1) * cPI2_8);
//           x[1]   = MULT_NORM((r0   - r1) * cPI2_8);

    vR = vec_sub(x24to27, x16to19);
    vR = vec_perm(vMinus, vR, permute9);  // r0 r1 r0 r1
    x24to27 = vec_add(x16to19, x24to27);
    x16to19 = vec_perm(vTmp, vR, permute10);
//           r0     = x[3]  - x[11];
//           r1     = x[10] - x[2];
//           x[10] += x[2];
//           x[11] += x[3];
//           x[2]   = r0;
//           x[3]   = r1;

    vMinus = vec_sub(x28to31, x20to23);
    vR = vec_perm(vMinus, vMinus, permute2);  // r0 r1 r0 r1
    x28to31 = vec_add(x28to31, x20to23);
//           r0     = x[12] - x[4];
//           r1     = x[13] - x[5];
//           x[12] += x[4];
//           x[13] += x[5];


    vTmp = vec_sld(vR, vR, 4);   // r1 r0 r1 r0
    vTmp2 = vec_add(vR, vTmp);   // r1+r0 r1+r0 r1+r0 r1+r0
    vTmp3 = vec_sub(vR, vTmp);   // r0-r1 r1-r0 r0-r1 r1-r0
    vTmp2 = vec_madd(vTmp2, vPi2, zero);
    vTmp3 = vec_madd(vTmp3, vPi2, zero);
    x20to23 = vec_sld(vTmp3, vTmp2, 8);  // x4 x x5 x   misordered!
//           x[4]   = MULT_NORM((r0   - r1) * cPI2_8);
//           x[5]   = MULT_NORM((r0   + r1) * cPI2_8);

    vR = vec_perm(vMinus, vMinus, permute7);
//           r0     = x[14] - x[6];
//           r1     = x[15] - x[7];
//           x[14] += x[6];
//           x[15] += x[7];

    x20to23 = vec_perm(x20to23, vR, permute11);
//           x[6]  = r0;
//           x[7]  = r1;

///  ----  butterfly8 (x+16)
    vMinus = vec_sub(x20to23, x16to19);
    vPlus = vec_add(x20to23, x16to19);

    vTmp = vec_sld(vPlus, vPlus, 8);
    vTmp2 = vec_add(vTmp, vPlus); // x6 x x x
    vTmp3 = vec_sub(vTmp, vPlus); // x4 x x x
    x20to23 = vec_perm(vTmp2, vTmp3, permute12);  // x4 x x6 x
//  r0   = x[6] + x[2];
//  r1   = x[6] - x[2];
//  r2   = x[4] + x[0];
//  r3   = x[4] - x[0];
//	   x[6] = r0   + r2;
//	   x[4] = r0   - r2;
	   
    vTmp = vec_sld(vMinus, vMinus, 4);
    vTmp2 = vec_add(vTmp, vMinus); // x x0 x x
    vTmp3 = vec_sub(vTmp, vMinus); // x x2 x x
    vTmp2 = vec_sld(vTmp2, vTmp2, 4);
    vTmp3 = vec_sld(vTmp3, vTmp3, 4);
    x16to19 = vec_perm(vTmp3, vTmp2, permute12);  // x0 x x2 x
//	   r0   = x[5] - x[1];
//	   r2   = x[7] - x[3];
//	   x[0] = r1   + r0;
//	   x[2] = r1   - r0;

    vTmp = vec_sld(vMinus, vMinus, 12);
    vTmp2 = vec_add(vTmp, vMinus); // x3 x x x
    vTmp3 = vec_sub(vTmp, vMinus); // x1 x x x
    vTmp = vec_perm(vTmp2, vTmp3, permute12);   // x1 x x3 x
    x16to19 = vec_perm(x16to19, vTmp, permute13);
//	   r0   = x[5] + x[1];
//	   r1   = x[7] + x[3];
//	   x[3] = r2   + r3;
//	   x[1] = r2   - r3;

    vTmp = vec_sld(vPlus, vPlus, 8);
    vTmp2 = vec_add(vTmp, vPlus); // x x7 x x
    vTmp3 = vec_sub(vTmp, vPlus); // x x5 x x
    vTmp2 = vec_sld(vTmp2, vTmp2, 4);
    vTmp3 = vec_sld(vTmp3, vTmp3, 4);
    vTmp = vec_perm(vTmp2, vTmp3, permute12);   // x5 x x7 x
    x20to23 = vec_perm(x20to23, vTmp, permute13);
    
//	   x[7] = r1   + r0;
//	   x[5] = r1   - r0;

///  ----  butterfly8 (x+24)
    vMinus = vec_sub(x28to31, x24to27);
    vPlus = vec_add(x28to31, x24to27);

    vTmp = vec_sld(vPlus, vPlus, 8);
    vTmp2 = vec_add(vTmp, vPlus); // x6 x x x
    vTmp3 = vec_sub(vTmp, vPlus); // x4 x x x
    x28to31 = vec_perm(vTmp2, vTmp3, permute12);  // x4 x x6 x
//  r0   = x[6] + x[2];
//  r1   = x[6] - x[2];
//  r2   = x[4] + x[0];
//  r3   = x[4] - x[0];
//	   x[6] = r0   + r2;
//	   x[4] = r0   - r2;
	   
    vTmp = vec_sld(vMinus, vMinus, 4);
    vTmp2 = vec_add(vTmp, vMinus); // x x0 x x
    vTmp3 = vec_sub(vTmp, vMinus); // x x2 x x
    vTmp2 = vec_sld(vTmp2, vTmp2, 4);
    vTmp3 = vec_sld(vTmp3, vTmp3, 4);
    x24to27 = vec_perm(vTmp3, vTmp2, permute12);  // x0 x x2 x
//	   r0   = x[5] - x[1];
//	   r2   = x[7] - x[3];
//	   x[0] = r1   + r0;
//	   x[2] = r1   - r0;

    vTmp = vec_sld(vMinus, vMinus, 12);
    vTmp2 = vec_add(vTmp, vMinus); // x3 x x x
    vTmp3 = vec_sub(vTmp, vMinus); // x1 x x x
    vTmp = vec_perm(vTmp2, vTmp3, permute12);   // x1 x x3 x
    x24to27 = vec_perm(x24to27, vTmp, permute13);
//	   r0   = x[5] + x[1];
//	   r1   = x[7] + x[3];
//	   x[3] = r2   + r3;
//	   x[1] = r2   - r3;

    vTmp = vec_sld(vPlus, vPlus, 8);
    vTmp2 = vec_add(vTmp, vPlus); // x x7 x x
    vTmp3 = vec_sub(vTmp, vPlus); // x x5 x x
    vTmp2 = vec_sld(vTmp2, vTmp2, 4);
    vTmp3 = vec_sld(vTmp3, vTmp3, 4);
    vTmp = vec_perm(vTmp2, vTmp3, permute12);   // x5 x x7 x
    x28to31 = vec_perm(x28to31, vTmp, permute13);
    
//	   x[7] = r1   + r0;
//	   x[5] = r1   - r0;

// ---

    vec_st(x0to3,   0x00, x);
    vec_st(x4to7,   0x10, x);
    vec_st(x8to11,  0x20, x);
    vec_st(x12to15, 0x30, x);
    vec_st(x16to19, 0x40, x);
    vec_st(x20to23, 0x50, x);
    vec_st(x24to27, 0x60, x);
    vec_st(x28to31, 0x70, x);
}

static void mdct_butterfly_generic_vectorized(float *_T, float *_x, int points, int _trigint)
{
  register float *x = _x;
  register float *T = _T;
  register float *x1        = x          + points      - 8;
  register float *x2        = x          + (points>>1) - 8;
  register int trigint = _trigint;

  register vector float vx1a;
  register vector float vx2a;
  register vector float vx1b;
  register vector float vx2b;
  register vector float vTmp1;
  register vector float vTmp2;
  register vector float vT1;
  register vector float vT2;
  register vector float vT3;
  register vector float vT4;
  register vector float vR;
  register vector float vMinus;
  register vector float vPlus;

  register vector unsigned char permute1 = VPERMUTE4(2, 3, 2, 3);
  register vector unsigned char permute2 = VPERMUTE4(0, 1, 0, 1);
  register vector unsigned char permute3 = VPERMUTE4(1, 0, 0, 1);
  register vector unsigned char permute4 = VPERMUTE4(0, 1, 7, 3);
  register vector unsigned char permute5 = VPERMUTE4(0, 1, 2, 5);
  register vector unsigned char permute6 = VPERMUTE4(7, 1, 2, 3);
  register vector unsigned char permute7 = VPERMUTE4(0, 5, 2, 3);

  register vector float zero = (vector float) vec_splat_u32(0);

  do{
    vx1a = vec_ld(0, x1);
    vx1b = vec_ld(16, x1);
    vx2a = vec_ld(0, x2);
    vx2b = vec_ld(16, x2);
    vT1 = vec_ld(0, T); T += trigint;
    vT2 = vec_ld(0, T); T += trigint;
    vT3 = vec_ld(0, T); T += trigint;
    vT4 = vec_ld(0, T); T += trigint;

    // x1  6  7  6  7
    // x2 -6 -7 +6 +7
    //    -----------
    vMinus = vec_sub(vx1b, vx2b);
    vPlus = vec_add(vx1b, vx2b);
    vx1b = vPlus;
    vec_st(vx1b, 16, x1);

    vR = vec_perm(vMinus, vMinus, permute1);  // r0 r1 r0 r1
    vT1 = vec_perm(vT1, vT1, permute3);       // T1 T0 T0 T1
    vT1 = vec_madd(vR, vT1, zero);     // r0T1 r1T0 r0T0 r1T1
    vTmp1 = vec_sld(vT1, vT1, 12);     // r1T1 r0T1 r1T0 r0T0
    vTmp2 = vec_sub(vT1, vTmp1);       // blah   r1T0-r0T1 blah      blah
    vTmp1 = vec_add(vT1, vTmp1);       // blah   blah      blah      r1T1+r0T0
    vx2b = vec_perm(vx2b, vTmp1, permute4);  // fill in x2[6]
    vx2b = vec_perm(vx2b, vTmp2, permute5);  // fill in x2[7]

           /*
	       r0      = x1[6]      -  x2[6];
	       r1      = x1[7]      -  x2[7];
	       x1[6]  += x2[6];
	       x1[7]  += x2[7];
	       x2[6]   = MULT_NORM(r1 * T[1]  +  r0 * T[0]);
	       x2[7]   = MULT_NORM(r1 * T[0]  -  r0 * T[1]);

	       T+=trigint;
           */

    vR = vec_perm(vMinus, vMinus, permute2);  // r0 r1 r0 r1
    vT2 = vec_perm(vT2, vT2, permute3);       // T1 T0 T0 T1
    vT2 = vec_madd(vR, vT2, zero);     // r0T1 r1T0 r0T0 r1T1
    vTmp1 = vec_sld(vT2, vT2, 12);     // r1T1 r0T1 r1T0 r0T0
    vTmp2 = vec_sub(vT2, vTmp1);       // blah   r1T0-r0T1 blah      blah
    vTmp1 = vec_add(vT2, vTmp1);       // blah   blah      blah      r1T1+r0T0
    vx2b = vec_perm(vx2b, vTmp1, permute6);  // fill in x2[4]
    vx2b = vec_perm(vx2b, vTmp2, permute7);  // fill in x2[5]
    vec_st(vx2b, 16, x2);

           /*
	       r0      = x1[4]      -  x2[4];
	       r1      = x1[5]      -  x2[5];
	       x1[4]  += x2[4];
	       x1[5]  += x2[5];
	       x2[4]   = MULT_NORM(r1 * T[1]  +  r0 * T[0]);
	       x2[5]   = MULT_NORM(r1 * T[0]  -  r0 * T[1]);
	       
	       T+=trigint;
           */


    // x1  2  3  2  3
    // x2 -2 -3 +2 +3
    //    -----------

    vMinus = vec_sub(vx1a, vx2a);
    vPlus = vec_add(vx1a, vx2a);
    vx1a = vPlus;
    vec_st(vx1a, 0, x1);
    x1 -= 8;

    vR = vec_perm(vMinus, vMinus, permute1);  // r0 r1 r0 r1
    vT3 = vec_perm(vT3, vT3, permute3);       // T1 T0 T0 T1
    vT3 = vec_madd(vR, vT3, zero);     // r0T1 r1T0 r0T0 r1T1
    vTmp1 = vec_sld(vT3, vT3, 12);     // r1T1 r0T1 r1T0 r0T0
    vTmp2 = vec_sub(vT3, vTmp1);       // blah   r1T0-r0T1 blah      blah
    vTmp1 = vec_add(vT3, vTmp1);       // blah   blah      blah      r1T1+r0T0
    vx2a = vec_perm(vx2a, vTmp1, permute4);  // fill in x2[2]
    vx2a = vec_perm(vx2a, vTmp2, permute5);  // fill in x2[3]

           /*
	       r0      = x1[2]      -  x2[2];
	       r1      = x1[3]      -  x2[3];
	       x1[2]  += x2[2];
	       x1[3]  += x2[3];
	       x2[2]   = MULT_NORM(r1 * T[1]  +  r0 * T[0]);
	       x2[3]   = MULT_NORM(r1 * T[0]  -  r0 * T[1]);
	       
	       T+=trigint;
           */

    vR = vec_perm(vMinus, vMinus, permute2);  // r0 r1 r0 r1
    vT4 = vec_perm(vT4, vT4, permute3);       // T1 T0 T0 T1
    vT4 = vec_madd(vR, vT4, zero);     // r0T1 r1T0 r0T0 r1T1
    vTmp1 = vec_sld(vT4, vT4, 12);     // r1T1 r0T1 r1T0 r0T0
    vTmp2 = vec_sub(vT4, vTmp1);       // blah   r1T0-r0T1 blah      blah
    vTmp1 = vec_add(vT4, vTmp1);       // blah   blah      blah      r1T1+r0T0
    vx2a = vec_perm(vx2a, vTmp1, permute6);  // fill in x2[0]
    vx2a = vec_perm(vx2a, vTmp2, permute7);  // fill in x2[1]
    vec_st(vx2a, 0, x2);

           /*
	       r0      = x1[0]      -  x2[0];
	       r1      = x1[1]      -  x2[1];
	       x1[0]  += x2[0];
	       x1[1]  += x2[1];
	       x2[0]   = MULT_NORM(r1 * T[1]  +  r0 * T[0]);
	       x2[1]   = MULT_NORM(r1 * T[0]  -  r0 * T[1]);

	       T+=trigint;
           */

    x2 -= 8;
  }while(x2>=x);
}


static void mdct_bitreverse_vectorized(mdct_lookup *init, DATA_TYPE *x){
  register int        n       = init->n;
  register int       *bit     = init->bitrev;
  register DATA_TYPE *w0      = x;
  register DATA_TYPE *w1      = x = w0+(n>>1);
  register DATA_TYPE *T       = init->trig+n;
  register DATA_TYPE *x0;
  register DATA_TYPE *x1;

  register vector unsigned char aligner;
  register vector float overflow;

  register vector float vx1;
  register vector float vx0;
  register vector float vw1;
  register vector float vw0;
  register vector float vR;
  register vector float vT;
  register vector float vTmp;
  register vector float vTmp2;
  register vector float vTmp3;
  register vector float vMinus;
  register vector float vPlus;

  register vector unsigned char permute1  = VPERMUTE4(0, 0, 5, 5);
  register vector unsigned char permute2  = VPERMUTE4(0, 1, 1, 0);
  register vector unsigned char permute3  = VPERMUTE4(2, 0, 4, 0);
  register vector unsigned char permute4  = VPERMUTE4(0, 1, 2, 5);
  register vector unsigned char permute5  = VPERMUTE4(0, 0, 1, 4);
  register vector unsigned char permute6  = VPERMUTE4(2, 7, 0, 0);
  register vector unsigned char permute7  = VPERMUTE4(2, 4, 2, 3);
  register vector unsigned char permute8  = VPERMUTE4(1, 4, 0, 0);
  register vector unsigned char permute9  = VPERMUTE4(0, 0, 1, 7);
  register vector unsigned char permute10 = VPERMUTE4(2, 3, 2, 4);

  register vector float zero = (vector float) vec_splat_u32(0);
  register vector float pointfive = (vector float) ( 0.5f, 0.5f, 0.5f, 0.5f );

  do{
    x0    = x+bit[0];
    x1    = x+bit[1];

    aligner = vec_lvsl(0, x0);
    vx0 = vec_ld(0, x0);
    vx1 = vec_ld(0, x1);
    vT = vec_ld(0, T);

    aligner = vec_lvsl(0, x0);
    overflow = vec_ld(16, x0);
    vx0 = vec_perm(vx0, overflow, aligner);

    vPlus = vec_add(vx0, vx1);
    vMinus = vec_sub(vx0, vx1);
    vR = vec_perm(vPlus, vMinus, permute1);  // r1 r1 r0 r0
    vTmp = vec_perm(vT, vT, permute2);       // T0 T1 T1 T0
    vTmp = vec_madd(vTmp, vR, zero);   // r1T0 r1T1 r0T1 r0T0
    vTmp2 = vec_sld(vTmp, vTmp, 8);
    vTmp3 = vec_add(vTmp, vTmp2);  // r1T0+r0T1 x x x
    vTmp = vec_sub(vTmp, vTmp2);   // x r1T1-r0T0 x x
    vR = vec_perm(vR, vTmp3, permute3); // r0 r1 r2 x     // !!! FIXME: Use more registers and less permutes.
    vR = vec_perm(vR, vTmp, permute4);  // r0 r1 r2 r3

    /*
    REG_TYPE  r0     = x0[1]  - x1[1];
    REG_TYPE  r1     = x0[0]  + x1[0];
    REG_TYPE  r2     = MULT_NORM(r1     * T[0]   + r0 * T[1]);
    REG_TYPE  r3     = MULT_NORM(r1     * T[1]   - r0 * T[0]);
    */

	w1    -= 4;

    vTmp2 = vec_madd(vPlus, pointfive, zero);   // xx r0 xx xx
    vTmp3 = vec_madd(vMinus, pointfive, zero);  // r1 xx xx xx

    /*
    r0     = HALVE(x0[1] + x1[1]);
    r1     = HALVE(x0[0] - x1[0]);
    */

    vTmp = vec_perm(vTmp2, vTmp3, permute5);   // xx    xx    r0    r1
    vTmp2 = vec_perm(vTmp, vR, permute6);      // r0    r3    xx    xx
    vTmp3 = vec_perm(vR, vTmp3, permute7);     // r2    r1    r2    r3
    vw0 = vec_add(vTmp, vTmp3);                // xxxxx xxxxx r0+r2 r1+r3
    vw1 = vec_sub(vTmp2, vTmp3);               // r0-r2 r3-r1 xxxxx xxxxx

	/*
    w0[0]  = r0     + r2;
    w1[2]  = r0     - r2;
    w0[1]  = r1     + r3;
    w1[3]  = r3     - r1;
    */

    x0     = x+bit[2];
    x1     = x+bit[3];

    vx0 = vec_ld(0, x0);
    vx1 = vec_ld(0, x1);

    aligner = vec_lvsl(0, x0);
    overflow = vec_ld(16, x0);
    vx0 = vec_perm(vx0, overflow, aligner);

    vT = vec_sld(vT, vT, 8); // T2 T3 T0 T1   (lets me reuse permute2).

    vPlus = vec_add(vx0, vx1);
    vMinus = vec_sub(vx0, vx1);
    vR = vec_perm(vPlus, vMinus, permute1);  // r1 r1 r0 r0
    vTmp = vec_perm(vT, vT, permute2);       // T2 T3 T3 T2
    vTmp = vec_madd(vTmp, vR, zero);   // r1T2 r1T3 r0T3 r0T2
    vTmp2 = vec_sld(vTmp, vTmp, 8);
    vTmp3 = vec_add(vTmp, vTmp2);  // r1T2+r0T3 x x x
    vTmp = vec_sub(vTmp, vTmp2);   // x r1T3-r0T2 x x
    vR = vec_perm(vR, vTmp3, permute3); // r0 r1 r2 x
    vR = vec_perm(vR, vTmp, permute4);  // r0 r1 r2 r3

    /*
    r0     = x0[1]  - x1[1];
    r1     = x0[0]  + x1[0];
    r2     = MULT_NORM(r1     * T[2]   + r0 * T[3]);
    r3     = MULT_NORM(r1     * T[3]   - r0 * T[2]);
    */

    vTmp2 = vec_madd(vPlus, pointfive, zero);   // xx r0 xx xx
    vTmp3 = vec_madd(vMinus, pointfive, zero);  // r1 xx xx xx

    /*
    r0     = HALVE(x0[1] + x1[1]);
    r1     = HALVE(x0[0] - x1[0]);
    */

    vTmp = vec_perm(vTmp2, vTmp3, permute8);   // r0    r1    xx    xx
    vTmp2 = vec_perm(vTmp2, vR, permute9);     // xx    xx    r0    r3
    vTmp3 = vec_perm(vR, vTmp3, permute10);    // r2    r3    r2    r1
    vTmp = vec_add(vTmp, vTmp3);               // r0+r2 r1+r3 xxxxx xxxxx
    vTmp2 = vec_sub(vTmp2, vTmp3);             // xxxxx xxxxx r0-r2 r3-r1

    vw0 = vec_sld(vw0, vTmp, 8);
    vw1 = vec_sld(vTmp2, vw1, 8);

    /*
    w0[2]  = r0     + r2;
    w1[0]  = r0     - r2;
    w0[3]  = r1     + r3;
    w1[1]  = r3     - r1;
    */

    vec_st(vw0, 0, w0);
    vec_st(vw1, 0, w1);

    T     += 4;
    bit   += 4;
    w0    += 4;
  }while(w0<w1);
}


static void mdct_backward_vectorized(mdct_lookup *init, DATA_TYPE *in, DATA_TYPE *out){
  register int n=init->n;
  register int n2=n>>1;
  register int n4=n>>2;

  /* rotate */

  register DATA_TYPE *iX = in+n2-7;
  register DATA_TYPE *oX = out+n2+n4;
  register DATA_TYPE *T  = init->trig+n4;

  register vector float voX1;
  register vector float voX2;
  register vector float vT1;
  register vector float vT2;
  register vector float vT3;
  register vector float viX1_orig;
  register vector float viX1;
  register vector float viX2;
  register vector float viX3;
  register vector float negative1 = (vector float) (-1.0f, 1.0f, -1.0f, 1.0f);
  register vector float negative2 = (vector float) (-1.0f, -1.0f, -1.0f, -1.0f);
  register vector float negative3 = (vector float) (1.0f, -1.0f, 1.0f, -1.0f);
  register vector float zero = (vector float) vec_splat_u32(0);

  // permute vectors to get things ordered for multiplications.
  register vector unsigned char aligner;
  register vector unsigned char permutevT1 = (vector unsigned char)
                                                vec_splat_u8(4);

  register vector unsigned char permutevT2 = VPERMUTE4(2, 2, 0, 0);
  register vector unsigned char permuteviX1 = VPERMUTE4(2, 0, 6, 4);
  register vector unsigned char permuteviX2 = VPERMUTE4(1, 0, 3, 2);

  // Skip the load from memory...add 4 to second permute vec.
  //  This might be a cycle or two faster.  :)
  permutevT1 = vec_add(permutevT1, permutevT2);

  // iX never appears to be aligned, and since it's frequently (always?)
  //  part of the same buffer as oX, we can't change this by altering the
  //  the caller to behave. We must align it manually.
  aligner = vec_lvsl(0, iX);
  viX3 = vec_ld(32, iX);
  do{
    oX -= 4;   // Move back 16 bytes.

    vT1 = vec_ld(0, T);
    viX1_orig = vec_ld(0, iX);
    viX2 = vec_ld(16, iX);

    iX         -= 8;
    T          += 4;

    viX1 = vec_perm(viX1_orig, viX2, aligner);
    viX2 = vec_perm(viX2, viX3, aligner);

    // Permute elements to this order for multiplication:
    //   viX    -2 +0 -6 +4   |   0  2  4  6
    //   vT      3  3  1  1   |   2  2  0  0
    // 1st vectors are multiplied, 2nd are multiplied and subtracted from 1st.
    vT2 = vec_perm(vT1, vT1, permutevT2);
    vT1 = vec_perm(vT1, vT1, permutevT1);
    viX1 = vec_perm(viX1, viX2, permuteviX1); // moves even elements to viX1
    viX2 = vec_perm(viX1, viX1, permuteviX2); // redistribute viX1 into v1X2
    viX1 = vec_madd(viX1, negative1, zero);  // flip iX elements to neg.
    viX1 = vec_madd(viX1, vT1, zero);  // flip iX elements to neg.
    voX1 = vec_nmsub(viX2, vT2, viX1);
    vec_st(voX1, 0, oX);
    viX3 = viX1_orig;  // move for next alignment permute.
  }while(iX>=in);

  iX            = in+n2-8;
  oX            = out+n2+n4;
  T             = init->trig+n4;

  permutevT1 = VPERMUTE4(3, 2, 1, 0);
  permutevT2 = VPERMUTE4(2, 3, 0, 1);
  permuteviX1 = VPERMUTE4(4, 4, 0, 0);

  // Skip the load from memory...add 8 to first permute vec.
  //  This might be a cycle or two faster.  :)
  permuteviX2 = (vector unsigned char) vec_splat_u8(8);
  permuteviX2 = vec_add(permuteviX1, permuteviX2);

  do{
    T          -= 4;

    vT1 = vec_ld(0, T);
    viX1 = vec_ld(0, iX);
    viX2 = vec_ld(16, iX);

    iX         -= 8;

    // Permute elements to this order for multiplication:
    //   viX    +4 +4 +0 +0   |  +6 -6 +2 -2
    //   vT      3  2  1  0   |   2  3  0  1
    // First vectors are multiplied and put into voX1, second vectors are
    //  multiplied and added to voX1.
    vT2 = vec_perm(vT1, vT1, permutevT2);
    vT1 = vec_perm(vT1, vT1, permutevT1);
    viX3 = vec_perm(viX1, viX2, permuteviX2); // redistribute viX1 into v1X2
    viX1 = vec_perm(viX1, viX2, permuteviX1); // moves even elements to viX1
    viX2 = vec_madd(viX3, negative3, zero);  // flip iX elements to neg.
    voX1 = vec_madd(viX1, vT1, zero);
    voX1 = vec_madd(viX2, vT2, voX1);
    vec_st(voX1, 0, oX);

    oX         += 4;
  }while(iX>=in);

  mdct_butterflies_vectorized(init,out+n2,n2);
  mdct_bitreverse_vectorized(init,out);

  /* roatate + window */

  {
    register DATA_TYPE *oX1=out+n2+n4;
    register DATA_TYPE *oX2=out+n2+n4;
    register DATA_TYPE *iX =out;
    T             =init->trig+n2;

    permutevT1 = VPERMUTE4(1, 3, 5, 7);
    permutevT2 = vec_sub(permutevT1, vec_splat_u8(4));
    permuteviX1 = VPERMUTE4(3, 2, 1, 0);

    do{
      oX1-=4;

      vT3 = vec_ld(0, T);
      vT2 = vec_ld(16, T);
      viX1 = vec_ld(0, iX);
      viX2 = vec_ld(16, iX);

      // Permute elements to this order for multiplication:
      //   viX    +0 +2 +4 +6 (viX1)  |  -1 -3 -5 -7  (viX3)
      //   vT      1  3  5  7 (vT1)   |   0  2  4  6  (vT2)
      //   viX    +0 +2 +4 +6 (viX1)  |  +1 +3 +5 +7  (viX2)
      //   vT      0  2  4  6 (vT2)   |   1  3  5  7  (vT1)
      // First vectors are multiplied and put into voX1, second vectors are
      //  multiplied and added to voX1. Second set lands in voX2.
      vT1 = vec_perm(vT3, vT2, permutevT1);
      vT2 = vec_perm(vT3, vT2, permutevT2);
      viX3 = vec_perm(viX1, viX2, permutevT1);   // 1 3 5 7
      viX1 = vec_perm(viX1, viX2, permutevT2);   // 0 2 4 6
      viX2 = viX3;                               // 1 3 5 7
      viX3 = vec_madd(viX3, negative2, zero);    // -1 -3 -5 -7
      voX1 = vec_madd(viX1, vT1, zero);
      voX1 = vec_madd(viX3, vT2, voX1);
      voX2 = vec_madd(viX1, vT2, zero);
      voX2 = vec_madd(viX2, vT1, voX2);
      voX2 = vec_madd(voX2, negative2, zero);  // make all 4 floats negative.
      vec_st(voX2, 0, oX2);
      // have to swap voX1 around.
      voX1 = vec_perm(voX1, voX1, permuteviX1);
      vec_st(voX1, 0, oX1);

      oX2+=4;
      iX    +=   8;
      T     +=   8;
    }while(iX<oX1);

    iX=out+n2+n4;
    oX1=out+n4;
    oX2=oX1;

    do{
      oX1-=4;
      iX-=4;

      viX1 = vec_ld(0, iX);
      voX2 = vec_madd(viX1, negative2, zero);
      voX2 = vec_perm(voX2, voX2, permuteviX1);  // swap 3210 to 0123
      vec_st(viX1, 0, oX1);
      vec_st(voX2, 0, oX2);

      oX2+=4;
    }while(oX2<iX);

    iX=out+n2+n4;
    oX1=out+n2+n4;
    oX2=out+n2;
    do{
      oX1-=4;
      viX1 = vec_ld(0, iX);
      viX1 = vec_perm(viX1, viX1, permuteviX1);  // swap 3210 to 0123
      vec_st(viX1, 0, oX1);
      iX+=4;
    }while(oX1>oX2);
  }
}


#else  /* no vectorized code for this platform. */

STIN void mdct_butterfly_first_vectorized(DATA_TYPE *T,DATA_TYPE *x,int points){
    /* Landing in here means something is broken. */
}

STIN void mdct_butterflies(mdct_lookup *init, DATA_TYPE *x, int points){
    /* Landing in here means something is broken. */
}

STIN void mdct_butterfly_8_vectorized(DATA_TYPE *_x){
    /* Landing in here means something is broken. */
}

STIN void mdct_butterfly_16_vectorized(DATA_TYPE *x){
    /* Landing in here means something is broken. */
}

STIN void mdct_butterfly_32_vectorized(DATA_TYPE *x){
    /* Landing in here means something is broken. */
}

STIN void mdct_bitreverse_vectorized(mdct_lookup *init, DATA_TYPE *x){
    /* Landing in here means something is broken. */
}

STIN void mdct_backward_vectorized(mdct_lookup *init, DATA_TYPE *in, DATA_TYPE *out){
    /* Landing in here means something is broken. */
}

STIN void mdct_butterfly_generic_vectorized(DATA_TYPE *T, DATA_TYPE *x, int points, int trigint) {
    /* Landing in here means something is broken. */
}

#endif


/* build lookups for trig functions; also pre-figure scaling and
   some window function algebra. */

void mdct_init(mdct_lookup *lookup,int n){
  int   *bitrev=_ogg_malloc(sizeof(*bitrev)*(n/4));
  DATA_TYPE *T=_ogg_malloc(sizeof(*T)*(n+n/4));
  
  int i;
  int n2=n>>1;
  int log2n=lookup->log2n=rint(log((float)n)/log(2.f));
  lookup->n=n;
  lookup->trig=T;
  lookup->bitrev=bitrev;

/* trig lookups... */

  for(i=0;i<n/4;i++){
    T[i*2]=FLOAT_CONV(cos((M_PI/n)*(4*i)));
    T[i*2+1]=FLOAT_CONV(-sin((M_PI/n)*(4*i)));
    T[n2+i*2]=FLOAT_CONV(cos((M_PI/(2*n))*(2*i+1)));
    T[n2+i*2+1]=FLOAT_CONV(sin((M_PI/(2*n))*(2*i+1)));
  }
  for(i=0;i<n/8;i++){
    T[n+i*2]=FLOAT_CONV(cos((M_PI/n)*(4*i+2))*.5);
    T[n+i*2+1]=FLOAT_CONV(-sin((M_PI/n)*(4*i+2))*.5);
  }

  /* bitreverse lookup... */

  {
    int mask=(1<<(log2n-1))-1,i,j;
    int msb=1<<(log2n-2);
    for(i=0;i<n/8;i++){
      int acc=0;
      for(j=0;msb>>j;j++)
	if((msb>>j)&i)acc|=1<<j;
      bitrev[i*2]=((~acc)&mask)-1;
      bitrev[i*2+1]=acc;

    }
  }
  lookup->scale=FLOAT_CONV(4.f/n);
}

/* 8 point butterfly (in place, 4 register) */
STIN void mdct_butterfly_8(DATA_TYPE *x){

  REG_TYPE r0;
  REG_TYPE r1;
  REG_TYPE r2;
  REG_TYPE r3;

  if (_vorbis_has_vector_unit()){
    mdct_butterfly_8_vectorized(x);
    return;
  }

  r0   = x[6] + x[2];
  r1   = x[6] - x[2];
  r2   = x[4] + x[0];
  r3   = x[4] - x[0];

	   x[6] = r0   + r2;
	   x[4] = r0   - r2;
	   
	   r0   = x[5] - x[1];
	   r2   = x[7] - x[3];
	   x[0] = r1   + r0;
	   x[2] = r1   - r0;
	   
	   r0   = x[5] + x[1];
	   r1   = x[7] + x[3];
	   x[3] = r2   + r3;
	   x[1] = r2   - r3;
	   x[7] = r1   + r0;
	   x[5] = r1   - r0;
	   
}

/* 16 point butterfly (in place, 4 register) */
STIN void mdct_butterfly_16(DATA_TYPE *x){
  REG_TYPE r0;
  REG_TYPE r1;

  if (_vorbis_has_vector_unit()){
    mdct_butterfly_16_vectorized(x);
    return;
  }

           r0     = x[1]  - x[9];
           r1     = x[0]  - x[8];

           x[8]  += x[0];
           x[9]  += x[1];
           x[0]   = MULT_NORM((r0   + r1) * cPI2_8);
           x[1]   = MULT_NORM((r0   - r1) * cPI2_8);

           r0     = x[3]  - x[11];
           r1     = x[10] - x[2];
           x[10] += x[2];
           x[11] += x[3];
           x[2]   = r0;
           x[3]   = r1;

           r0     = x[12] - x[4];
           r1     = x[13] - x[5];
           x[12] += x[4];
           x[13] += x[5];
           x[4]   = MULT_NORM((r0   - r1) * cPI2_8);
           x[5]   = MULT_NORM((r0   + r1) * cPI2_8);

           r0     = x[14] - x[6];
           r1     = x[15] - x[7];
           x[14] += x[6];
           x[15] += x[7];
           x[6]  = r0;
           x[7]  = r1;

	   mdct_butterfly_8(x);
	   mdct_butterfly_8(x+8);
}


/* 32 point butterfly (in place, 4 register) */
static void mdct_butterfly_32_scalar(DATA_TYPE *x){
  REG_TYPE r0     = x[30] - x[14];
  REG_TYPE r1     = x[31] - x[15];

           x[30] +=         x[14];           
	   x[31] +=         x[15];
           x[14]  =         r0;              
	   x[15]  =         r1;

           r0     = x[28] - x[12];   
	   r1     = x[29] - x[13];
           x[28] +=         x[12];           
	   x[29] +=         x[13];
           x[12]  = MULT_NORM( r0 * cPI1_8  -  r1 * cPI3_8 );
	   x[13]  = MULT_NORM( r0 * cPI3_8  +  r1 * cPI1_8 );

           r0     = x[26] - x[10];
	   r1     = x[27] - x[11];
	   x[26] +=         x[10];
	   x[27] +=         x[11];
	   x[10]  = MULT_NORM(( r0  - r1 ) * cPI2_8);
	   x[11]  = MULT_NORM(( r0  + r1 ) * cPI2_8);

	   r0     = x[24] - x[8];
	   r1     = x[25] - x[9];
	   x[24] += x[8];
	   x[25] += x[9];
	   x[8]   = MULT_NORM( r0 * cPI3_8  -  r1 * cPI1_8 );
	   x[9]   = MULT_NORM( r1 * cPI3_8  +  r0 * cPI1_8 );

	   r0     = x[22] - x[6];
	   r1     = x[7]  - x[23];
	   x[22] += x[6];
	   x[23] += x[7];
	   x[6]   = r1;
	   x[7]   = r0;

	   r0     = x[4]  - x[20];
	   r1     = x[5]  - x[21];
	   x[20] += x[4];
	   x[21] += x[5];
	   x[4]   = MULT_NORM( r1 * cPI1_8  +  r0 * cPI3_8 );
	   x[5]   = MULT_NORM( r1 * cPI3_8  -  r0 * cPI1_8 );

	   r0     = x[2]  - x[18];
	   r1     = x[3]  - x[19];
	   x[18] += x[2];
	   x[19] += x[3];
	   x[2]   = MULT_NORM(( r1  + r0 ) * cPI2_8);
	   x[3]   = MULT_NORM(( r1  - r0 ) * cPI2_8);

	   r0     = x[0]  - x[16];
	   r1     = x[1]  - x[17];
	   x[16] += x[0];
	   x[17] += x[1];
	   x[0]   = MULT_NORM( r1 * cPI3_8  +  r0 * cPI1_8 );
	   x[1]   = MULT_NORM( r1 * cPI1_8  -  r0 * cPI3_8 );

	   mdct_butterfly_16(x);
	   mdct_butterfly_16(x+16);

}

// had to split this up to prevent call to saveFP on MacOSX. --ryan.
STIN void mdct_butterfly_32(DATA_TYPE *x){
  if (_vorbis_has_vector_unit()){
    mdct_butterfly_32_vectorized(x);
  } else {
    mdct_butterfly_32_scalar(x);
  }
}


/* N point first stage butterfly (in place, 2 register) */
STIN void mdct_butterfly_first(DATA_TYPE *T,
					DATA_TYPE *x,
					int points){
  
  DATA_TYPE *x1        = x          + points      - 8;
  DATA_TYPE *x2        = x          + (points>>1) - 8;
  REG_TYPE   r0;
  REG_TYPE   r1;

  do{
    
               r0      = x1[6]      -  x2[6];
	       r1      = x1[7]      -  x2[7];
	       x1[6]  += x2[6];
	       x1[7]  += x2[7];
	       x2[6]   = MULT_NORM(r1 * T[1]  +  r0 * T[0]);
	       x2[7]   = MULT_NORM(r1 * T[0]  -  r0 * T[1]);
	       
	       r0      = x1[4]      -  x2[4];
	       r1      = x1[5]      -  x2[5];
	       x1[4]  += x2[4];
	       x1[5]  += x2[5];
	       x2[4]   = MULT_NORM(r1 * T[5]  +  r0 * T[4]);
	       x2[5]   = MULT_NORM(r1 * T[4]  -  r0 * T[5]);
	       
	       r0      = x1[2]      -  x2[2];
	       r1      = x1[3]      -  x2[3];
	       x1[2]  += x2[2];
	       x1[3]  += x2[3];
	       x2[2]   = MULT_NORM(r1 * T[9]  +  r0 * T[8]);
	       x2[3]   = MULT_NORM(r1 * T[8]  -  r0 * T[9]);
	       
	       r0      = x1[0]      -  x2[0];
	       r1      = x1[1]      -  x2[1];
	       x1[0]  += x2[0];
	       x1[1]  += x2[1];
	       x2[0]   = MULT_NORM(r1 * T[13] +  r0 * T[12]);
	       x2[1]   = MULT_NORM(r1 * T[12] -  r0 * T[13]);
	       
    x1-=8;
    x2-=8;
    T+=16;

  }while(x2>=x);
}


/* N/stage point generic N stage butterfly (in place, 2 register) */
STIN void mdct_butterfly_generic(DATA_TYPE *T,
					  DATA_TYPE *x,
					  int points,
					  int trigint){

  DATA_TYPE *x1;
  DATA_TYPE *x2;
  REG_TYPE   r0;
  REG_TYPE   r1;

  if ( (_vorbis_has_vector_unit()) ){
    mdct_butterfly_generic_vectorized(T, x, points, trigint);
    return;
  }

  x1        = x          + points      - 8;
  x2        = x          + (points>>1) - 8;

  do{
	       r0      = x1[6]      -  x2[6];
	       r1      = x1[7]      -  x2[7];
	       x1[6]  += x2[6];
	       x1[7]  += x2[7];
	       x2[6]   = MULT_NORM(r1 * T[1]  +  r0 * T[0]);
	       x2[7]   = MULT_NORM(r1 * T[0]  -  r0 * T[1]);
	       
	       T+=trigint;
	       
	       r0      = x1[4]      -  x2[4];
	       r1      = x1[5]      -  x2[5];
	       x1[4]  += x2[4];
	       x1[5]  += x2[5];
	       x2[4]   = MULT_NORM(r1 * T[1]  +  r0 * T[0]);
	       x2[5]   = MULT_NORM(r1 * T[0]  -  r0 * T[1]);
	       
	       T+=trigint;
	       
	       r0      = x1[2]      -  x2[2];
	       r1      = x1[3]      -  x2[3];
	       x1[2]  += x2[2];
	       x1[3]  += x2[3];
	       x2[2]   = MULT_NORM(r1 * T[1]  +  r0 * T[0]);
	       x2[3]   = MULT_NORM(r1 * T[0]  -  r0 * T[1]);
	       
	       T+=trigint;
	       
	       r0      = x1[0]      -  x2[0];
	       r1      = x1[1]      -  x2[1];
	       x1[0]  += x2[0];
	       x1[1]  += x2[1];
	       x2[0]   = MULT_NORM(r1 * T[1]  +  r0 * T[0]);
	       x2[1]   = MULT_NORM(r1 * T[0]  -  r0 * T[1]);

	       T+=trigint;
    x1-=8;
    x2-=8;

  }while(x2>=x);
}


STIN void mdct_butterflies_vectorized(mdct_lookup *init,
			     DATA_TYPE *x,
			     int points){
  register DATA_TYPE *T=init->trig;
  register int stages=init->log2n-5;
  register int i,j;
  
  if(--stages>0){
    mdct_butterfly_first_vectorized(T,x,points);
  }

  for(i=1;--stages>0;i++){
    register int fouri = 4<<i;
    register int pointsi = points>>i;
    register int max = 1<<i;
    for(j=0;j<max;j++)
      mdct_butterfly_generic_vectorized(T,x+(pointsi)*j,pointsi,fouri);
  }

  for(j=0;j<points;j+=32)
    mdct_butterfly_32_vectorized(x+j);
}

STIN void mdct_butterflies_scalar(mdct_lookup *init,
			     DATA_TYPE *x,
			     int points){
  register DATA_TYPE *T=init->trig;
  register int stages=init->log2n-5;
  register int i,j;
  
  if(--stages>0){
    mdct_butterfly_first(T,x,points);
  }

  for(i=1;--stages>0;i++){
    register int fouri = 4<<i;
    register int pointsi = points>>i;
    register int max = 1<<i;
    for(j=0;j<max;j++)
      mdct_butterfly_generic(T,x+(pointsi)*j,pointsi,fouri);
  }

  for(j=0;j<points;j+=32)
    mdct_butterfly_32(x+j);
}


STIN void mdct_butterflies(mdct_lookup *init,
			     DATA_TYPE *x,
			     int points){
  
  if ( _vorbis_has_vector_unit() ){
    mdct_butterflies_vectorized(init, x, points);
  } else {
    mdct_butterflies_scalar(init, x, points);
  }
}

void mdct_clear(mdct_lookup *l){
  if(l){
    if(l->trig)_ogg_free(l->trig);
    if(l->bitrev)_ogg_free(l->bitrev);
    memset(l,0,sizeof(*l));
  }
}

STIN void mdct_bitreverse(mdct_lookup *init, 
			    DATA_TYPE *x){

  int        n;
  int       *bit;
  DATA_TYPE *w0;
  DATA_TYPE *w1;
  DATA_TYPE *T;

  if (_vorbis_has_vector_unit()) {
    mdct_bitreverse_vectorized(init, x);
    return;
  }

  n       = init->n;
  bit     = init->bitrev;
  w0      = x;
  w1      = x = w0+(n>>1);
  T       = init->trig+n;

  do{
    DATA_TYPE *x0    = x+bit[0];
    DATA_TYPE *x1    = x+bit[1];

    REG_TYPE  r0     = x0[1]  - x1[1];
    REG_TYPE  r1     = x0[0]  + x1[0];
    REG_TYPE  r2     = MULT_NORM(r1     * T[0]   + r0 * T[1]);
    REG_TYPE  r3     = MULT_NORM(r1     * T[1]   - r0 * T[0]);

	      w1    -= 4;

              r0     = HALVE(x0[1] + x1[1]);
              r1     = HALVE(x0[0] - x1[0]);
      
	      w0[0]  = r0     + r2;
	      w1[2]  = r0     - r2;
	      w0[1]  = r1     + r3;
	      w1[3]  = r3     - r1;

              x0     = x+bit[2];
              x1     = x+bit[3];

              r0     = x0[1]  - x1[1];
              r1     = x0[0]  + x1[0];
              r2     = MULT_NORM(r1     * T[2]   + r0 * T[3]);
              r3     = MULT_NORM(r1     * T[3]   - r0 * T[2]);

              r0     = HALVE(x0[1] + x1[1]);
              r1     = HALVE(x0[0] - x1[0]);
      
	      w0[2]  = r0     + r2;
	      w1[0]  = r0     - r2;
	      w0[3]  = r1     + r3;
	      w1[1]  = r3     - r1;

	      T     += 4;
	      bit   += 4;
	      w0    += 4;

  }while(w0<w1);
}

void mdct_backward(mdct_lookup *init, DATA_TYPE *in, DATA_TYPE *out){
  int n=init->n;
  int n2=n>>1;
  int n4=n>>2;

  /* rotate */

  DATA_TYPE *iX = in+n2-7;
  DATA_TYPE *oX = out+n2+n4;
  DATA_TYPE *T  = init->trig+n4;

  if ( (_vorbis_has_vector_unit()) &&
       ( !(UNALIGNED_PTR(T) || UNALIGNED_PTR(oX) || UNALIGNED_PTR(out)) ) ) {
    mdct_backward_vectorized(init, in, out);
    return;
  }

  do{
    oX         -= 4;
    oX[0]       = MULT_NORM(-iX[2] * T[3] - iX[0]  * T[2]);
    oX[1]       = MULT_NORM (iX[0] * T[3] - iX[2]  * T[2]);
    oX[2]       = MULT_NORM(-iX[6] * T[1] - iX[4]  * T[0]);
    oX[3]       = MULT_NORM (iX[4] * T[1] - iX[6]  * T[0]);
    iX         -= 8;
    T          += 4;
  }while(iX>=in);

  iX            = in+n2-8;
  oX            = out+n2+n4;
  T             = init->trig+n4;

  do{
    T          -= 4;
    oX[0]       =  MULT_NORM (iX[4] * T[3] + iX[6] * T[2]);
    oX[1]       =  MULT_NORM (iX[4] * T[2] - iX[6] * T[3]);
    oX[2]       =  MULT_NORM (iX[0] * T[1] + iX[2] * T[0]);
    oX[3]       =  MULT_NORM (iX[0] * T[0] - iX[2] * T[1]);
    iX         -= 8;
    oX         += 4;
  }while(iX>=in);

  mdct_butterflies(init,out+n2,n2);
  mdct_bitreverse(init,out);

  {
    DATA_TYPE *oX1=out+n2+n4;
    DATA_TYPE *oX2=out+n2+n4;
    DATA_TYPE *iX =out;
    T             =init->trig+n2;
    
    do{
      oX1-=4;

      oX1[3]  =  MULT_NORM (iX[0] * T[1] - iX[1] * T[0]);
      oX2[0]  = -MULT_NORM (iX[0] * T[0] + iX[1] * T[1]);

      oX1[2]  =  MULT_NORM (iX[2] * T[3] - iX[3] * T[2]);
      oX2[1]  = -MULT_NORM (iX[2] * T[2] + iX[3] * T[3]);

      oX1[1]  =  MULT_NORM (iX[4] * T[5] - iX[5] * T[4]);
      oX2[2]  = -MULT_NORM (iX[4] * T[4] + iX[5] * T[5]);

      oX1[0]  =  MULT_NORM (iX[6] * T[7] - iX[7] * T[6]);
      oX2[3]  = -MULT_NORM (iX[6] * T[6] + iX[7] * T[7]);

      oX2+=4;
      iX    +=   8;
      T     +=   8;
    }while(iX<oX1);

    iX=out+n2+n4;
    oX1=out+n4;
    oX2=oX1;

    do{
      oX1-=4;
      iX-=4;

      oX2[0] = -(oX1[3] = iX[3]);
      oX2[1] = -(oX1[2] = iX[2]);
      oX2[2] = -(oX1[1] = iX[1]);
      oX2[3] = -(oX1[0] = iX[0]);

      oX2+=4;
    }while(oX2<iX);

    iX=out+n2+n4;
    oX1=out+n2+n4;
    oX2=out+n2;
    do{
      oX1-=4;
      oX1[0]= iX[3];
      oX1[1]= iX[2];
      oX1[2]= iX[1];
      oX1[3]= iX[0];
      iX+=4;
    }while(oX1>oX2);
  }
}

void mdct_forward(mdct_lookup *init, DATA_TYPE *in, DATA_TYPE *out){
  int n=init->n;
  int n2=n>>1;
  int n4=n>>2;
  int n8=n>>3;
  DATA_TYPE *w=alloca(n*sizeof(*w)); /* forward needs working space */
  DATA_TYPE *w2=w+n2;

  /* rotate */

  /* window + rotate + step 1 */
  
  REG_TYPE r0;
  REG_TYPE r1;
  DATA_TYPE *x0=in+n2+n4;
  DATA_TYPE *x1=x0+1;
  DATA_TYPE *T=init->trig+n2;
  
  int i=0;
  
  for(i=0;i<n8;i+=2){
    x0 -=4;
    T-=2;
    r0= x0[2] + x1[0];
    r1= x0[0] + x1[2];       
    w2[i]=   MULT_NORM(r1*T[1] + r0*T[0]);
    w2[i+1]= MULT_NORM(r1*T[0] - r0*T[1]);
    x1 +=4;
  }

  x1=in+1;
  
  for(;i<n2-n8;i+=2){
    T-=2;
    x0 -=4;
    r0= x0[2] - x1[0];
    r1= x0[0] - x1[2];       
    w2[i]=   MULT_NORM(r1*T[1] + r0*T[0]);
    w2[i+1]= MULT_NORM(r1*T[0] - r0*T[1]);
    x1 +=4;
  }
    
  x0=in+n;

  for(;i<n2;i+=2){
    T-=2;
    x0 -=4;
    r0= -x0[2] - x1[0];
    r1= -x0[0] - x1[2];       
    w2[i]=   MULT_NORM(r1*T[1] + r0*T[0]);
    w2[i+1]= MULT_NORM(r1*T[0] - r0*T[1]);
    x1 +=4;
  }


  mdct_butterflies(init,w+n2,n2);
  mdct_bitreverse(init,w);

  /* roatate + window */

  T=init->trig+n2;
  x0=out+n2;

  for(i=0;i<n4;i++){
    x0--;
    out[i] =MULT_NORM((w[0]*T[0]+w[1]*T[1])*init->scale);
    x0[0]  =MULT_NORM((w[0]*T[1]-w[1]*T[0])*init->scale);
    w+=2;
    T+=2;
  }
}

