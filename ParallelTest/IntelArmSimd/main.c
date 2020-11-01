#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#define SIMD "Intel SSE/AVX"
#include <intrin.h>
#elif defined(__ARM_NEON)
#define SIMD "ARM NEON"
#include "sse2neon.h"	// https://github.com/DLTcollab/sse2neon
#else
#error Unknown platform!
#endif

#define print_array(_array) {for (int i = 0; i < sizeof(_array) / sizeof(_array[0]); ++i) { printf("%4d", _array[i]); }	printf("\n");}

#define ENABLE_SIMD

void simple128bitSimdTest() {

	{
		static const int DIM = 4;
		unsigned int s0[DIM] = { 1, 2, 3, 4 };
		unsigned int s1[DIM] = { 10,20,30,40 };
		unsigned int d0[DIM] = { 0 };

		print_array(s0);
		print_array(s1);

#if defined(ENABLE_SIMD)
		__m128i s0m128i = _mm_load_si128((__m128i*)s0);
		__m128i s1m128i = _mm_load_si128((__m128i*)s1);
		__m128i d0m128i = _mm_add_epi32(s0m128i, s1m128i);
		_mm_store_si128((__m128i*)d0, d0m128i);
#else
		for (int i = 0; i < DIM; ++i) {
			d0[i] = s0[i] + s1[i];
		}
#endif

		print_array(d0);
	}

	{
		static const int DIM = 8;
		unsigned short s0[DIM] = { 1, 2, 3, 4, 5, 6, 7, 8 };
		unsigned short s1[DIM] = { 10,20,30,40,50,60,70,80 };
		unsigned short d0[DIM] = { 0 };

		print_array(s0);
		print_array(s1);

#if defined(ENABLE_SIMD)
		__m128i s0m128i = _mm_load_si128((__m128i*)s0);
		__m128i s1m128i = _mm_load_si128((__m128i*)s1);
		__m128i d0m128i = _mm_add_epi16(s0m128i, s1m128i);
		_mm_store_si128((__m128i*)d0, d0m128i);
#else
		for (int i = 0; i < DIM; ++i) {
			d0[i] = s0[i] + s1[i];
		}
#endif

		print_array(d0);
	}

	{
		static const int DIM = 16;
		unsigned char s0[DIM] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
		unsigned char s1[DIM] = { 10,20,30,40,50,60,70,80,90,100,110,120,130,140,150,160 };
		unsigned char d0[DIM] = { 0 };

		print_array(s0);
		print_array(s1);

#if defined(ENABLE_SIMD)
		__m128i s0m128i = _mm_load_si128((__m128i*)s0);
		__m128i s1m128i = _mm_load_si128((__m128i*)s1);
		__m128i d0m128i = _mm_add_epi8(s0m128i, s1m128i);
		_mm_store_si128((__m128i*)d0, d0m128i);
#else
		for (int i = 0; i < DIM; ++i) {
			d0[i] = s0[i] + s1[i];
		}
#endif

		print_array(d0);
	}
}

int main(int argc, char* argv[]) {

	printf("SIMD: %s\n", SIMD);

	simple128bitSimdTest();

	return 0;
}