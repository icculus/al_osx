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

#define CLAMP(x, l, h) if (x < l) x = l; else if (x > h) x = h;

// Mac users with G5's should set the USE_G5_OPCODES option in the makefile.
//  This will use a faster, more precise square root calculation and use
//  Altivec in places we wouldn't if we needed to check for availability, but
//  the shared library will crash on a G4 or older CPU.
//
// Mac users with G4's should set the USE_G4_OPCODES option in the makefile.
//  This will use Altivec in places we wouldn't if we needed to check for
//  availability, but the shared library will crash on a G3 or older CPU.
//
// Please note that if you don't set the USE_x_OPCODES makefile options, we'll
//  still try to do the right thing, including detecting Altivec support and
//  using it when it makes sense to do so...USE_G4_OPCODES just allows us to
//  use _more_ Altivec, since we can guarantee your processor has it.

// 4 for Altivec-happiness, but we only need 3.
typedef ALfloat ALvector[4] __attribute__((aligned(16)));

#define DIVBY32767 0.000030519f
#define DIVBY127   0.007874016f

static inline ALfloat __alRecipEstimate(ALfloat x)
{
#if 0 //MACOSX
    __asm__ __volatile__ (
        "fres %0, %1  \n\t"
            : "=f" (x) : "f" (x)
    );
    return(x);
#else
    return(1.0f / x);
#endif
}

#define EPSILON_EQUAL(x, y) ( ((x)>=((y)-0.0000001)) && ((x)<=((y)+0.0000001)) )

// !!! FIXME: Is there not a cosf/sinf/etc on MacOSX?
#if MACOSX
#define sinf(f) ((float) sin((double) f))
#define cosf(f) ((float) cos((double) f))
#define tanf(f) ((float) tan((double) f))
#define asinf(f) ((float) asin((double) f))
#define acosf(f) ((float) acos((double) f))
#define atanf(f) ((float) atan((double) f))
#define atan2f(x,y) ((float) atan2((double) x, (double) y))
#endif

static inline ALvoid __alSinCos(ALfloat f, ALfloat *s, ALfloat *c)
{
    // !!! FIXME: use fsincos opcode on x86.
    *s = sinf(f);
    *c = cosf(f);
} // __alSinCos


// Square root stuff.
#if (!defined FORCE_STD_SQRT)
#  if (!MACOSX)
#    define FORCE_STD_SQRT 1
#  endif
#endif

#if FORCE_STD_SQRT   // This become an fsqrt opcode with USE_G5_OPCODES.
#   define __alSqrt(x) ((ALfloat) sqrt(x))
#else
// Square root from reciprocal sqrt estimate opcode, plus newton-raphson.
// Compliments of Jeffrey Lim, in regards to something unrelated.
//  Says him:
//   "I did some testing, and the fsqrte is very inaccurate, as you said -
//    but after 2 iterations of newton-raphson, it seems to be accurate to
//     1 ULP of the answer provided by sqrt()".
// This is WAY faster than libSystem's sqrt(), but not as fast (or
//  precise) as the G5's fsqrt opcode.
static inline Float32 __alSqrt(Float32 num)
{
    register Float32 t1;
    register Float32 t2;
    register Float32 t3;
    register Float32 hx;

    if (num == 0.0f)
        return(0.0f);

    __asm__ __volatile__ (
        "frsqrte %0, %1  \n\t"
            : "=f" (t1) : "f" (num)
    );

    hx = num * 0.5f;
    t2 = t1 * (1.5f - hx * t1 * t1);
    t3 = t2 * (1.5f - hx * t2 * t2);

    return(t3 * num);
} // __alSqrt
#endif


static inline ALvoid __alCrossproduct(ALvector vector1, ALvector vector2, ALvector vector3)
{
    // !!! FIXME: vectorize?
    vector3[0]=(vector1[1]*vector2[2]-vector1[2]*vector2[1]);
	vector3[1]=(vector1[2]*vector2[0]-vector1[0]*vector2[2]);
    vector3[2]=(vector1[0]*vector2[1]-vector1[1]*vector2[0]);
} // __alCrossproduct


static inline ALfloat __alDotproduct(ALvector vector1,ALvector vector2)
{
#if 1  // !!! FIXME: this is actually faster than Altivec version.
    return (vector1[0]*vector2[0]+vector1[1]*vector2[1]+vector1[2]*vector2[2]);
#else
    register vector float zero = (vector float) vec_splat_u32(0);
    register vector float v1 = vec_ld(0x00, vector1);
    register vector float v2 = vec_ld(0x00, vector2);

    union { vector float v; float f[4]; } swapper;
    v1 = vec_madd(v1, v2, zero);
    v2 = vec_add(v1, vec_sld(v1, v1, 4));
    v2 = vec_add(v2, vec_sld(v1, v1, 8));
    vec_st(v2, 0x00, &swapper.v);
    return(swapper.f[0]);
#endif
}


static inline ALvoid __alNormalize(ALvector v)
{
	register ALfloat len = __alRecipEstimate(__alSqrt(__alDotproduct(v, v)));
	v[0] *= len;
	v[1] *= len;
	v[2] *= len;
} // __alNormalize


static inline ALvoid __alVectorAssign3(ALvector v1, ALvector v2)
{
#if FORCE_ALTIVEC
    register vector float v = vec_ld(0x00, v2);
    vec_st(v, 0x00, v1);
#else // slower, but doesn't crash on G3.
    v1[0] = v2[0];
    v1[1] = v2[1];
    v1[2] = v2[2];
#endif
} // __alVectorAssign3


static inline ALvoid __alVectorAssign4(ALvector v1, ALvector v2)
{
#if FORCE_ALTIVEC
    register vector float v = vec_ld(0x00, v2);
    vec_st(v, 0x00, v1);
#else // slower, but doesn't crash on G3.
    v1[0] = v2[0];
    v1[1] = v2[1];
    v1[2] = v2[2];
    v1[3] = v2[3];
#endif
} // __alVectorAssign4


static inline ALvoid __alVectorSubtract3(ALvector v1, ALvector v2)
{
#if FORCE_ALTIVEC
    register vector float vec1 = vec_ld(0x00, v1);
    register vector float vec2 = vec_ld(0x00, v2);
    vec1 = vec_sub(vec1, vec2);
    vec_st(vec1, 0x00, v1);
#else  // slower, but doesn't crash on G3.
    v1[0] -= v2[0];
    v1[1] -= v2[1];
    v1[2] -= v2[2];
#endif
} // __alVectorSubtract3


static inline ALvoid __alVectorSubtract4(ALvector v1, ALvector v2)
{
#if FORCE_ALTIVEC
    register vector float vec1 = vec_ld(0x00, v1);
    register vector float vec2 = vec_ld(0x00, v2);
    vec1 = vec_sub(vec1, vec2);
    vec_st(vec1, 0x00, v1);
#else  // slower, but doesn't crash on G3.
    v1[0] -= v2[0];
    v1[1] -= v2[1];
    v1[2] -= v2[2];
    v1[3] -= v2[3];
#endif
} // __alVectorSubtract4


// !!! FIXME: vectorize!
static inline ALvoid __alMatrixVector(ALfloat matrix[3][3], ALvector v)
{
	ALvector result;

	result[0]=v[0]*matrix[0][0]+v[1]*matrix[1][0]+v[2]*matrix[2][0];
	result[1]=v[0]*matrix[0][1]+v[1]*matrix[1][1]+v[2]*matrix[2][1];
	result[2]=v[0]*matrix[0][2]+v[1]*matrix[1][2]+v[2]*matrix[2][2];

    __alVectorAssign3(v, result);
} // __alMatrixVector

// end of alMath.h ...


