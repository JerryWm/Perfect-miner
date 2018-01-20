/*-
 * Copyright 2009 Colin Percival
 * Copyright 2012-2014 Alexander Peslyak
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file was originally written by Colin Percival as part of the Tarsnap
 * online backup system.
 */

/*
 * On 64-bit, enabling SSE4.1 helps our pwxform code indirectly, via avoiding
 * gcc bug 54349 (fixed for gcc 4.9+).  On 32-bit, it's of direct help.  AVX
 * and XOP are of further help either way.
 */
 
#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "nostdlib.c"
#include "sha256_c.h"
#include "sysendian.h"

#include <emmintrin.h>
#ifdef __XOP__
	#include <x86intrin.h>
#endif

#if __STDC_VERSION__ >= 199901L
	/* have restrict */
#elif defined(__GNUC__)
	#define restrict __restrict
#else
	#define restrict
#endif

#define PREFETCH(x, hint) _mm_prefetch((const char *)(x), (hint));
#define PREFETCH_OUT(x, hint) /* disabled */

#define PREFETCH_0(x)	\
	PREFETCH(x, _MM_HINT_T0)

#ifdef __XOP__
	#define ARX(out, in1, in2, s) \
		out = _mm_xor_si128(out, _mm_roti_epi32(_mm_add_epi32(in1, in2), s));
#else
	#define ARX(out, in1, in2, s) \
		{ \
			__m128i T = _mm_add_epi32(in1, in2); \
			out = _mm_xor_si128(out, _mm_slli_epi32(T, s)); \
			out = _mm_xor_si128(out, _mm_srli_epi32(T, 32-s)); \
		}
#endif

#define SALSA20_2ROUNDS \
	/* Operate on "columns" */ \
	ARX(X1, X0, X3, 7) \
	ARX(X2, X1, X0, 9) \
	ARX(X3, X2, X1, 13) \
	ARX(X0, X3, X2, 18) \
\
	/* Rearrange data */ \
	X1 = _mm_shuffle_epi32(X1, 0x93); \
	X2 = _mm_shuffle_epi32(X2, 0x4E); \
	X3 = _mm_shuffle_epi32(X3, 0x39); \
\
	/* Operate on "rows" */ \
	ARX(X3, X0, X1, 7) \
	ARX(X2, X3, X0, 9) \
	ARX(X1, X2, X3, 13) \
	ARX(X0, X1, X2, 18) \
\
	/* Rearrange data */ \
	X1 = _mm_shuffle_epi32(X1, 0x39); \
	X2 = _mm_shuffle_epi32(X2, 0x4E); \
	X3 = _mm_shuffle_epi32(X3, 0x93);

/**
 * Apply the salsa20/8 core to the block provided in (X0 ... X3).
 */
#define SALSA20_8_BASE(maybe_decl, out) \
	{ \
		maybe_decl Y0 = X0; \
		maybe_decl Y1 = X1; \
		maybe_decl Y2 = X2; \
		maybe_decl Y3 = X3; \
		SALSA20_2ROUNDS \
		SALSA20_2ROUNDS \
		SALSA20_2ROUNDS \
		SALSA20_2ROUNDS \
		(out)[0] = X0 = _mm_add_epi32(X0, Y0); \
		(out)[1] = X1 = _mm_add_epi32(X1, Y1); \
		(out)[2] = X2 = _mm_add_epi32(X2, Y2); \
		(out)[3] = X3 = _mm_add_epi32(X3, Y3); \
	}
#define SALSA20_8(out) \
	SALSA20_8_BASE(__m128i, out)

/**
 * Apply the salsa20/8 core to the block provided in (X0 ... X3) ^ (Z0 ... Z3).
 */
#define SALSA20_8_XOR_ANY(maybe_decl, Z0, Z1, Z2, Z3, out) \
	X0 = _mm_xor_si128(X0, Z0); \
	X1 = _mm_xor_si128(X1, Z1); \
	X2 = _mm_xor_si128(X2, Z2); \
	X3 = _mm_xor_si128(X3, Z3); \
	SALSA20_8_BASE(maybe_decl, out)

#define SALSA20_8_XOR_MEM(in, out) \
	SALSA20_8_XOR_ANY(__m128i, (in)[0], (in)[1], (in)[2], (in)[3], out)

#define SALSA20_8_XOR_REG(out) \
	SALSA20_8_XOR_ANY(/* empty */, Y0, Y1, Y2, Y3, out)

typedef union {
	uint32_t w[16];
	__m128i q[4];
} salsa20_blk_t;


static inline void blockmix_salsa8(const salsa20_blk_t *restrict Bin, salsa20_blk_t *restrict Bout, size_t r) {
	__m128i X0, X1, X2, X3;
	size_t i;

	r--;
	PREFETCH(&Bin[r * 2 + 1], _MM_HINT_T0)
	for (i = 0; i < r; i++) {
		PREFETCH(&Bin[i * 2], _MM_HINT_T0)
		PREFETCH_OUT(&Bout[i], _MM_HINT_T0)
		PREFETCH(&Bin[i * 2 + 1], _MM_HINT_T0)
		PREFETCH_OUT(&Bout[r + 1 + i], _MM_HINT_T0)
	}
	PREFETCH(&Bin[r * 2], _MM_HINT_T0)
	PREFETCH_OUT(&Bout[r], _MM_HINT_T0)
	PREFETCH_OUT(&Bout[r * 2 + 1], _MM_HINT_T0)

	X0 = Bin[r * 2 + 1].q[0];
	X1 = Bin[r * 2 + 1].q[1];
	X2 = Bin[r * 2 + 1].q[2];
	X3 = Bin[r * 2 + 1].q[3];

	SALSA20_8_XOR_MEM(Bin[0].q, Bout[0].q)

	for (i = 0; i < r;) {
		SALSA20_8_XOR_MEM(Bin[i * 2 + 1].q, Bout[r + 1 + i].q)

		i++;

		SALSA20_8_XOR_MEM(Bin[i * 2].q, Bout[i].q)
	}

	SALSA20_8_XOR_MEM(Bin[r * 2 + 1].q, Bout[r * 2 + 1].q)
}

#if 1
	#define HI32(X) \
		_mm_shuffle_epi32((X), _MM_SHUFFLE(2,3,0,1))
#elif 0
	#define HI32(X) \
		_mm_srli_si128((X), 4)
#else
	#define HI32(X) \
		_mm_srli_epi64((X), 32)
#endif

#if defined(__x86_64__) && (defined(__ICC) || defined(__llvm__))
	/* Intel's name, also supported by recent gcc */
	#define EXTRACT64(X) _mm_cvtsi128_si64(X)
#elif defined(__x86_64__) && !defined(_MSC_VER) && !defined(__OPEN64__)
	/* gcc got the 'x' name earlier than non-'x', MSVC and Open64 had bugs */
	#define EXTRACT64(X) _mm_cvtsi128_si64x(X)
#elif defined(__x86_64__) && defined(__SSE4_1__)
	/* No known bugs for this intrinsic */
	#include <smmintrin.h>
	#define EXTRACT64(X) _mm_extract_epi64((X), 0)
#elif defined(__SSE4_1__)
	/* 32-bit */
	#include <smmintrin.h>
	#if 0
	/* This is currently unused by the code below, which instead uses these two
	 * intrinsics explicitly when (!defined(__x86_64__) && defined(__SSE4_1__)) */
		#define EXTRACT64(X) \
			((uint64_t)(uint32_t)_mm_cvtsi128_si32(X) | \
			((uint64_t)(uint32_t)_mm_extract_epi32((X), 1) << 32))
	#endif
#else
	/* 32-bit or compilers with known past bugs in _mm_cvtsi128_si64*() */
	#define EXTRACT64(X) \
		((uint64_t)(uint32_t)_mm_cvtsi128_si32(X) | \
		((uint64_t)(uint32_t)_mm_cvtsi128_si32(HI32(X)) << 32))
#endif

#define S_BITS 8
#define S_SIMD 2
#define S_P 4
#define S_N 2

#define S_SIZE1 (1 << S_BITS)
#define S_MASK ((S_SIZE1 - 1) * S_SIMD * 8)
#define _S_MASK2 (((uint64_t)S_MASK << 32) | S_MASK)
#define S_SIZE_ALL (S_N * S_SIZE1 * S_SIMD * 8)

uint64_t volatile S_MASK2_GBVAR = _S_MASK2;
#define S_MASK2		_S_MASK2

#if !defined(__x86_64__) && defined(__SSE4_1__)
	/* 32-bit with SSE4.1 */
	#define PWXFORM_X_T __m128i
	#define PWXFORM_SIMD(X, x, s0, s1) \
		x = _mm_and_si128(X, _mm_set1_epi64x(S_MASK2)); \
		s0 = *(const __m128i *)(S0 + (uint32_t)_mm_cvtsi128_si32(x)); \
		s1 = *(const __m128i *)(S1 + (uint32_t)_mm_extract_epi32(x, 1)); \
		X = _mm_mul_epu32(HI32(X), X); \
		X = _mm_add_epi64(X, s0); \
		X = _mm_xor_si128(X, s1);
#else
	/* 64-bit, or 32-bit without SSE4.1 */
	#define PWXFORM_X_T uint64_t
	#define PWXFORM_SIMD(X, x, s0, s1) \
		x = EXTRACT64(X) & S_MASK2_GBVAR; \
		s0 = *(const __m128i *)(S0 + (uint32_t)x); \
		s1 = *(const __m128i *)(S1 + (x >> 32)); \
		X = _mm_mul_epu32(HI32(X), X); \
		X = _mm_add_epi64(X, s0); \
		X = _mm_xor_si128(X, s1);
#endif

#define PWXFORM_ROUND \
	PWXFORM_SIMD(X0, x0, s00, s01) \
	PWXFORM_SIMD(X1, x1, s10, s11) \
	PWXFORM_SIMD(X2, x2, s20, s21) \
	PWXFORM_SIMD(X3, x3, s30, s31)


	
#if 1

	#define PWXFORM \
		{ \
			uint64_t volatile S_MASK2_GBVAR = 17523466571760;	\
			PWXFORM_X_T x0, x1, x2, x3; \
			__m128i s00, s01, s10, s11, s20, s21, s30, s31; \
			PWXFORM_ROUND PWXFORM_ROUND \
			PWXFORM_ROUND PWXFORM_ROUND \
			PWXFORM_ROUND PWXFORM_ROUND \
		}

#else
/**	
	#define PWXFORM \
		{ \
			PWXFORM_X_T x0, x1, x2, x3; \
			__m128i s00, s01, s10, s11, s20, s21, s30, s31; \
			PWXFORM_SIMD(X0, x0, s00, s01) PWXFORM_SIMD(X0, x0, s00, s01) PWXFORM_SIMD(X0, x0, s00, s01) PWXFORM_SIMD(X0, x0, s00, s01) PWXFORM_SIMD(X0, x0, s00, s01) PWXFORM_SIMD(X0, x0, s00, s01) \
			PWXFORM_SIMD(X1, x1, s10, s11) PWXFORM_SIMD(X1, x1, s10, s11) PWXFORM_SIMD(X1, x1, s10, s11) PWXFORM_SIMD(X1, x1, s10, s11) PWXFORM_SIMD(X1, x1, s10, s11) PWXFORM_SIMD(X1, x1, s10, s11)  \
			PWXFORM_SIMD(X2, x2, s20, s21) PWXFORM_SIMD(X2, x2, s20, s21) PWXFORM_SIMD(X2, x2, s20, s21) PWXFORM_SIMD(X2, x2, s20, s21) PWXFORM_SIMD(X2, x2, s20, s21) PWXFORM_SIMD(X2, x2, s20, s21)  \
			PWXFORM_SIMD(X3, x3, s30, s31) PWXFORM_SIMD(X3, x3, s30, s31) PWXFORM_SIMD(X3, x3, s30, s31) PWXFORM_SIMD(X3, x3, s30, s31) PWXFORM_SIMD(X3, x3, s30, s31) PWXFORM_SIMD(X3, x3, s30, s31)  \
		}
*/
#endif

#define XOR4(in) \
	X0 = _mm_xor_si128(X0, (in)[0]); \
	X1 = _mm_xor_si128(X1, (in)[1]); \
	X2 = _mm_xor_si128(X2, (in)[2]); \
	X3 = _mm_xor_si128(X3, (in)[3]);

#undef OUT
#define OUT(out) \
	(out)[0] = X0; \
	(out)[1] = X1; \
	(out)[2] = X2; \
	(out)[3] = X3;

static void blockmix(const salsa20_blk_t *restrict Bin, salsa20_blk_t *restrict Bout, size_t r, const __m128i *restrict S) {
	const uint8_t * S0, * S1;
	__m128i X0, X1, X2, X3;
	size_t i;

	if (!S) {
		blockmix_salsa8(Bin, Bout, r);
		return;
	}

	S0 = (const uint8_t *)S;
	S1 = (const uint8_t *)S + S_SIZE_ALL / 2;

	/* Convert 128-byte blocks to 64-byte blocks */
	r *= 2;

	r--;
	PREFETCH(&Bin[r], _MM_HINT_T0)
	for (i = 0; i < r; i++) {
		PREFETCH(&Bin[i], _MM_HINT_T0)
		PREFETCH_OUT(&Bout[i], _MM_HINT_T0)
	}
	PREFETCH_OUT(&Bout[r], _MM_HINT_T0)

	/* X <-- B_{r1 - 1} */
	X0 = Bin[r].q[0];
	X1 = Bin[r].q[1];
	X2 = Bin[r].q[2];
	X3 = Bin[r].q[3];

	/* for i = 0 to r1 - 1 do */
	for (i = 0; i < r; i++) {
		/* X <-- H'(X \xor B_i) */
		XOR4(Bin[i].q)
		PWXFORM
		/* B'_i <-- X */
		OUT(Bout[i].q)
	}

	/* Last iteration of the loop above */
	XOR4(Bin[i].q)
	PWXFORM

	/* B'_i <-- H(B'_i) */
	SALSA20_8(Bout[i].q)
}

#define XOR4_2(in1, in2) \
	X0 = _mm_xor_si128((in1)[0], (in2)[0]); \
	X1 = _mm_xor_si128((in1)[1], (in2)[1]); \
	X2 = _mm_xor_si128((in1)[2], (in2)[2]); \
	X3 = _mm_xor_si128((in1)[3], (in2)[3]);

static inline uint32_t blockmix_salsa8_xor(const salsa20_blk_t *restrict Bin1, const salsa20_blk_t *restrict Bin2, salsa20_blk_t *restrict Bout, size_t r, int Bin2_in_ROM) {
	__m128i X0, X1, X2, X3;
	size_t i;

	r--;
	if (Bin2_in_ROM) {
		PREFETCH(&Bin2[r * 2 + 1], _MM_HINT_NTA)
		PREFETCH(&Bin1[r * 2 + 1], _MM_HINT_T0)
		for (i = 0; i < r; i++) {
			PREFETCH(&Bin2[i * 2], _MM_HINT_NTA)
			PREFETCH(&Bin1[i * 2], _MM_HINT_T0)
			PREFETCH(&Bin2[i * 2 + 1], _MM_HINT_NTA)
			PREFETCH(&Bin1[i * 2 + 1], _MM_HINT_T0)
			PREFETCH_OUT(&Bout[i], _MM_HINT_T0)
			PREFETCH_OUT(&Bout[r + 1 + i], _MM_HINT_T0)
		}
		PREFETCH(&Bin2[r * 2], _MM_HINT_T0)
	} else {
		PREFETCH(&Bin2[r * 2 + 1], _MM_HINT_T0)
		PREFETCH(&Bin1[r * 2 + 1], _MM_HINT_T0)
		for (i = 0; i < r; i++) {
			PREFETCH(&Bin2[i * 2], _MM_HINT_T0)
			PREFETCH(&Bin1[i * 2], _MM_HINT_T0)
			PREFETCH(&Bin2[i * 2 + 1], _MM_HINT_T0)
			PREFETCH(&Bin1[i * 2 + 1], _MM_HINT_T0)
			PREFETCH_OUT(&Bout[i], _MM_HINT_T0)
			PREFETCH_OUT(&Bout[r + 1 + i], _MM_HINT_T0)
		}
		PREFETCH(&Bin2[r * 2], _MM_HINT_T0)
	}
	PREFETCH(&Bin1[r * 2], _MM_HINT_T0)
	PREFETCH_OUT(&Bout[r], _MM_HINT_T0)
	PREFETCH_OUT(&Bout[r * 2 + 1], _MM_HINT_T0)

	/* 1: X <-- B_{2r - 1} */
	XOR4_2(Bin1[r * 2 + 1].q, Bin2[r * 2 + 1].q)

	/* 3: X <-- H(X \xor B_i) */
	/* 4: Y_i <-- X */
	/* 6: B' <-- (Y_0, Y_2 ... Y_{2r-2}, Y_1, Y_3 ... Y_{2r-1}) */
	XOR4(Bin1[0].q)
	SALSA20_8_XOR_MEM(Bin2[0].q, Bout[0].q)

	/* 2: for i = 0 to 2r - 1 do */
	for (i = 0; i < r;) {
		/* 3: X <-- H(X \xor B_i) */
		/* 4: Y_i <-- X */
		/* 6: B' <-- (Y_0, Y_2 ... Y_{2r-2}, Y_1, Y_3 ... Y_{2r-1}) */
		XOR4(Bin1[i * 2 + 1].q)
		SALSA20_8_XOR_MEM(Bin2[i * 2 + 1].q, Bout[r + 1 + i].q)

		i++;

		/* 3: X <-- H(X \xor B_i) */
		/* 4: Y_i <-- X */
		/* 6: B' <-- (Y_0, Y_2 ... Y_{2r-2}, Y_1, Y_3 ... Y_{2r-1}) */
		XOR4(Bin1[i * 2].q)
		SALSA20_8_XOR_MEM(Bin2[i * 2].q, Bout[i].q)
	}

	/* 3: X <-- H(X \xor B_i) */
	/* 4: Y_i <-- X */
	/* 6: B' <-- (Y_0, Y_2 ... Y_{2r-2}, Y_1, Y_3 ... Y_{2r-1}) */
	XOR4(Bin1[r * 2 + 1].q)
	SALSA20_8_XOR_MEM(Bin2[r * 2 + 1].q, Bout[r * 2 + 1].q)

	return _mm_cvtsi128_si32(X0);
}

//#include "asm_pwxform.c"

static uint32_t blockmix_xor(const salsa20_blk_t *restrict Bin1, const salsa20_blk_t *restrict Bin2, salsa20_blk_t *restrict Bout, size_t r, int Bin2_in_ROM, const __m128i *restrict S) {
	const uint8_t * S0, * S1;
	__m128i X0, X1, X2, X3;
	size_t i;

	if (!S) {
		return blockmix_salsa8_xor(Bin1, Bin2, Bout, r, Bin2_in_ROM);
	}

	S0 = (const uint8_t *)S;
	S1 = (const uint8_t *)S + S_SIZE_ALL / 2;

	/* Convert 128-byte blocks to 64-byte blocks */
	r *= 2;

	r--;
	
	
	if (Bin2_in_ROM) {
		PREFETCH(&Bin2[r], _MM_HINT_NTA)
		PREFETCH(&Bin1[r], _MM_HINT_T0)
		for (i = 0; i < r; i++) {
			PREFETCH(&Bin2[i], _MM_HINT_NTA)
			PREFETCH(&Bin1[i], _MM_HINT_T0)
			PREFETCH_OUT(&Bout[i], _MM_HINT_T0)
		}
	} else {
		PREFETCH(&Bin2[r], _MM_HINT_T0)
		PREFETCH(&Bin1[r], _MM_HINT_T0)
		for (i = 0; i < r; i++) {
			PREFETCH(&Bin2[i], _MM_HINT_T0)
			PREFETCH(&Bin1[i], _MM_HINT_T0)
			PREFETCH_OUT(&Bout[i], _MM_HINT_T0)
		}
	}
	PREFETCH_OUT(&Bout[r], _MM_HINT_T0);
	
#if 0
	
	asm_blockmix_xor(S0, S1, (uint8_t*)Bin1, (uint8_t*)Bin2, (uint8_t*)Bout);
	X0 = Bout[31].q[0];
	
#else
	
	XOR4_2(Bin1[31].q, Bin2[31].q)
	//asm("int $3");
	for (i = 0; i < 31; i++) {
		XOR4(Bin1[i].q)
		XOR4(Bin2[i].q)
			
		PWXFORM
		
		//MM_PWXFORM((uint8_t*)S0, (uint8_t*)S1, X0, X1, X2, X3);	
		OUT(Bout[i].q)
	}
	XOR4(Bin1[31].q)
	XOR4(Bin2[31].q)
	PWXFORM

	/* B'_i <-- H(B'_i) */
	SALSA20_8(Bout[31].q)	
#endif

	return _mm_cvtsi128_si32(X0);
}

#undef XOR4
#define XOR4(in, out) \
	(out)[0] = Y0 = _mm_xor_si128((in)[0], (out)[0]); \
	(out)[1] = Y1 = _mm_xor_si128((in)[1], (out)[1]); \
	(out)[2] = Y2 = _mm_xor_si128((in)[2], (out)[2]); \
	(out)[3] = Y3 = _mm_xor_si128((in)[3], (out)[3]);

static inline uint32_t blockmix_salsa8_xor_save(const salsa20_blk_t *restrict Bin1, salsa20_blk_t *restrict Bin2, salsa20_blk_t *restrict Bout, size_t r) {
	__m128i X0, X1, X2, X3, Y0, Y1, Y2, Y3;
	size_t i;

	r--;
	PREFETCH(&Bin2[r * 2 + 1], _MM_HINT_T0)
	PREFETCH(&Bin1[r * 2 + 1], _MM_HINT_T0)
	for (i = 0; i < r; i++) {
		PREFETCH(&Bin2[i * 2], _MM_HINT_T0)
		PREFETCH(&Bin1[i * 2], _MM_HINT_T0)
		PREFETCH(&Bin2[i * 2 + 1], _MM_HINT_T0)
		PREFETCH(&Bin1[i * 2 + 1], _MM_HINT_T0)
		PREFETCH_OUT(&Bout[i], _MM_HINT_T0)
		PREFETCH_OUT(&Bout[r + 1 + i], _MM_HINT_T0)
	}
	PREFETCH(&Bin2[r * 2], _MM_HINT_T0)
	PREFETCH(&Bin1[r * 2], _MM_HINT_T0)
	PREFETCH_OUT(&Bout[r], _MM_HINT_T0)
	PREFETCH_OUT(&Bout[r * 2 + 1], _MM_HINT_T0)

	/* 1: X <-- B_{2r - 1} */
	XOR4_2(Bin1[r * 2 + 1].q, Bin2[r * 2 + 1].q)

	/* 3: X <-- H(X \xor B_i) */
	/* 4: Y_i <-- X */
	/* 6: B' <-- (Y_0, Y_2 ... Y_{2r-2}, Y_1, Y_3 ... Y_{2r-1}) */
	XOR4(Bin1[0].q, Bin2[0].q)
	SALSA20_8_XOR_REG(Bout[0].q)

	/* 2: for i = 0 to 2r - 1 do */
	for (i = 0; i < r;) {
		/* 3: X <-- H(X \xor B_i) */
		/* 4: Y_i <-- X */
		/* 6: B' <-- (Y_0, Y_2 ... Y_{2r-2}, Y_1, Y_3 ... Y_{2r-1}) */
		XOR4(Bin1[i * 2 + 1].q, Bin2[i * 2 + 1].q)
		SALSA20_8_XOR_REG(Bout[r + 1 + i].q)

		i++;

		/* 3: X <-- H(X \xor B_i) */
		/* 4: Y_i <-- X */
		/* 6: B' <-- (Y_0, Y_2 ... Y_{2r-2}, Y_1, Y_3 ... Y_{2r-1}) */
		XOR4(Bin1[i * 2].q, Bin2[i * 2].q)
		SALSA20_8_XOR_REG(Bout[i].q)
	}

	/* 3: X <-- H(X \xor B_i) */
	/* 4: Y_i <-- X */
	/* 6: B' <-- (Y_0, Y_2 ... Y_{2r-2}, Y_1, Y_3 ... Y_{2r-1}) */
	XOR4(Bin1[r * 2 + 1].q, Bin2[r * 2 + 1].q)
	SALSA20_8_XOR_REG(Bout[r * 2 + 1].q)

	return _mm_cvtsi128_si32(X0);
}

#define XOR4_Y \
	X0 = _mm_xor_si128(X0, Y0); \
	X1 = _mm_xor_si128(X1, Y1); \
	X2 = _mm_xor_si128(X2, Y2); \
	X3 = _mm_xor_si128(X3, Y3);

static uint32_t blockmix_xor_save(const salsa20_blk_t *restrict Bin1, salsa20_blk_t *restrict Bin2, salsa20_blk_t *restrict Bout, size_t r, const __m128i *restrict S) {
	const uint8_t * S0, * S1;
	__m128i X0, X1, X2, X3, Y0, Y1, Y2, Y3;
	size_t i;

	if (!S)
		return blockmix_salsa8_xor_save(Bin1, Bin2, Bout, r);

	S0 = (const uint8_t *)S;
	S1 = (const uint8_t *)S + S_SIZE_ALL / 2;

	/* Convert 128-byte blocks to 64-byte blocks */
	r *= 2;

	r--;
	PREFETCH(&Bin2[r], _MM_HINT_T0)
	PREFETCH(&Bin1[r], _MM_HINT_T0)
	for (i = 0; i < r; i++) {
		PREFETCH(&Bin2[i], _MM_HINT_T0)
		PREFETCH(&Bin1[i], _MM_HINT_T0)
		PREFETCH_OUT(&Bout[i], _MM_HINT_T0)
	}
	PREFETCH_OUT(&Bout[r], _MM_HINT_T0);

	/* X <-- B_{r1 - 1} */
	XOR4_2(Bin1[r].q, Bin2[r].q)

	/* for i = 0 to r1 - 1 do */
	for (i = 0; i < r; i++) {
		XOR4(Bin1[i].q, Bin2[i].q)
		/* X <-- H'(X \xor B_i) */
		XOR4_Y
		PWXFORM
		/* B'_i <-- X */
		OUT(Bout[i].q)
	}

	/* Last iteration of the loop above */
	XOR4(Bin1[i].q, Bin2[i].q)
	XOR4_Y
	PWXFORM

	/* B'_i <-- H(B'_i) */
	SALSA20_8(Bout[i].q)

	return _mm_cvtsi128_si32(X0);
}

#undef ARX
#undef SALSA20_2ROUNDS
#undef SALSA20_8
#undef SALSA20_8_XOR_ANY
#undef SALSA20_8_XOR_MEM
#undef SALSA20_8_XOR_REG
#undef PWXFORM_SIMD_1
#undef PWXFORM_SIMD_2
#undef PWXFORM_ROUND
#undef PWXFORM
#undef OUT
#undef XOR4
#undef XOR4_2
#undef XOR4_Y


static inline void salsaTransformEnter(salsa20_blk_t * X, uint8_t *B, int r) {
	for (int k = 0; k < 2 * r; k++) {
		for (int i = 0; i < 16; i++) {
			X[k].w[i] = le32dec(&B[(k * 16 + (i * 5 % 16)) * 4]);
		}
	}
}
static inline void salsaTransformLeave(salsa20_blk_t * X, uint8_t *B, int r) {
	for (int k = 0; k < 2 * r; k++) {
		for (int i = 0; i < 16; i++) {
			le32enc(&B[(k * 16 + (i * 5 % 16)) * 4], X[k].w[i]);
		}
	}
}
static inline uint32_t integerify(const salsa20_blk_t * B, size_t r) {
	return B[2 * r - 1].w[0];
}
static void smix1(uint8_t * B, size_t r, uint32_t N, salsa20_blk_t * V, uint32_t NROM, salsa20_blk_t * XY, void * S) {
	size_t s = 2 * r;
	salsa20_blk_t * X = V, * Y;
	uint32_t i, j;
	size_t k;

	salsaTransformEnter(X, B, r);

	uint32_t n;
	salsa20_blk_t * V_n, * V_j;

	Y = &V[s];
	blockmix(X, Y, r, (const __m128i*)S);

	X = &V[2 * s];
	blockmix(Y, X, r, (const __m128i*)S);
	j = integerify(X, r);

	for (n = 2; n < N; n <<= 1) {
		uint32_t m = (n < N / 2) ? n : (N - 1 - n);

		V_n = &V[n * s];

		for (i = 1; i < m; i += 2) {
			Y = &V_n[i * s];

			j &= n - 1;
			j += i - 1;
			V_j = &V[j * s];

			j = blockmix_xor(X, V_j, Y, r, 0, (const __m128i*)S);

			j &= n - 1;
			j += i;
			V_j = &V[j * s];

			X = &V_n[(i + 1) * s];
			j = blockmix_xor(Y, V_j, X, r, 0, (const __m128i*)S);
		}
	}

	n >>= 1;

	j &= n - 1;
	j += N - 2 - n;
	V_j = &V[j * s];

	Y = &V[(N - 1) * s];
	j = blockmix_xor(X, V_j, Y, r, 0, (const __m128i*)S);

	j &= n - 1;
	j += N - 1 - n;
	V_j = &V[j * s];

	X = XY;
	blockmix_xor(Y, V_j, X, r, 0, (const __m128i*)S);

	salsaTransformLeave(X, B, r);
}
static void smix2(uint8_t * B, size_t r, uint32_t N, uint64_t Nloop, salsa20_blk_t * V, uint32_t NROM, salsa20_blk_t * XY, void * S) {
	size_t s = 2 * r;
	salsa20_blk_t * X = XY, * Y = &XY[s];
	uint64_t i;
	uint32_t j;
	size_t k;

	salsaTransformEnter(X, B, r);

	i = Nloop / 2;
	j = integerify(X, r) & (N - 1);

	do {
		salsa20_blk_t * V_j = &V[j * s];

		j = blockmix_xor_save(X, V_j, Y, r, (const __m128i*)S);
		j &= N - 1;
		V_j = &V[j * s];

		j = blockmix_xor_save(Y, V_j, X, r, (const __m128i*)S);
		j &= N - 1;
	} while (--i);

	salsaTransformLeave(X, B, r);
}
static void smix(uint8_t * B, salsa20_blk_t * V, salsa20_blk_t * XY, void * S) {
	size_t s = 2 * YESCRYPT_R;
	uint64_t Nloop_all, Nloop_rw;
	uint32_t i;
	
	Nloop_all = (YESCRYPT_N + 2) / 3; /* 1/3, round up */

	Nloop_rw = Nloop_all;

	Nloop_all++; 
	Nloop_all &= ~(uint64_t)1; /* round up to even */
	Nloop_rw &= ~(uint64_t)1; /* round down to even */

	smix1(B, 1, S_SIZE_ALL / 128, (salsa20_blk_t *)S, 0, XY, NULL);
	
	smix1(B, YESCRYPT_R, YESCRYPT_N, V, 0, XY, S);

	smix2(B, YESCRYPT_R, YESCRYPT_N, Nloop_rw, V, 0, XY, S);
}
static void yescrypt_r16_kdf(void *memory, const uint8_t * passwd, size_t passwdlen, const uint8_t * salt, size_t saltlen, uint8_t * buf, size_t buflen) {
	size_t B_size, V_size, XY_size;
	uint8_t * B, * S;
	salsa20_blk_t * V, * XY;
	uint8_t sha256[32];

	V_size = (size_t)128 * 16 * 4096;
	B_size = (size_t)128 * 16;
	XY_size = (size_t)256 * 16;

	B = (uint8_t *)memory;
	V  = (salsa20_blk_t *)((uint8_t *)B + B_size);
	XY = (salsa20_blk_t *)((uint8_t *)V + V_size);
	S = (uint8_t *)XY + XY_size;

	SHA256_HASH(&sha256, passwd, passwdlen);
	passwd = sha256;
	passwdlen = sizeof(sha256);
	
	PBKDF2_SHA256(passwd, passwdlen, salt, saltlen, 1, B, B_size);
	
	MEMCOPY(&sha256, B, sizeof(sha256));

	smix(B, V, XY, S);

	PBKDF2_SHA256(passwd, passwdlen, B, B_size, 1, buf, buflen);
	
	HMAC_SHA256_HASH(&sha256, buf, buflen, "Client Key", 10);
	SHA256_HASH(buf, &sha256, sizeof(sha256));
}



