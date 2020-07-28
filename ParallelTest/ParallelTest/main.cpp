#include <stdio.h>
#include <vector>
#include <algorithm>

#include "AppInfo.h"
#include "CpuInfo.h"
#include "Clock.h"

#include <immintrin.h>	// AVX2

#define CLIP(v) ((v<0?0:(255<v?255:v)))

#define MAT_SIZE 8
#define MAT_HEIGHT	MAT_SIZE
#define MAT_WIDTH	MAT_SIZE

void mul(const float a[MAT_HEIGHT][MAT_WIDTH], const float b[MAT_HEIGHT][MAT_WIDTH], float c[MAT_HEIGHT][MAT_WIDTH]) {
	for (uint8_t j = 0; j < MAT_HEIGHT; ++j) {
		for (uint8_t i = 0; i < MAT_WIDTH; ++i) {
			float sum = 0;
			for (uint8_t k = 0; k < MAT_HEIGHT; ++k) {
				sum += a[j][k] * b[k][i];
			}
			c[j][i] = sum;
		}
	}
}

void mulavx2(const float a[MAT_HEIGHT][MAT_WIDTH], const float b[MAT_HEIGHT][MAT_WIDTH], float c[MAT_HEIGHT][MAT_WIDTH]) {
	for (uint8_t j = 0; j < MAT_HEIGHT; ++j) {
		for (uint8_t i = 0; i < MAT_WIDTH; ++i) {
			__m256 a256 = _mm256_load_ps(a[j]);
			__m256 b256 = _mm256_set_ps(b[7][i], b[6][i], b[5][i], b[4][i], b[3][i], b[2][i], b[1][i], b[0][i]);
			__m256 dp256 = _mm256_dp_ps(a256, b256, 255);
			c[j][i] = dp256.m256_f32[0] + dp256.m256_f32[4];
		}
	}
}

void print(const float a[MAT_HEIGHT][MAT_WIDTH]) {
	for (uint8_t j = 0; j < MAT_HEIGHT; ++j) {
		for (uint8_t i = 0; i < MAT_WIDTH; ++i) {
			printf("%lf, ", a[j][i]);
		}
		printf("\n");
	}
	printf("\n");
}

void tanni(float a[MAT_HEIGHT][MAT_WIDTH]) {
	for (uint8_t j = 0; j < MAT_HEIGHT; ++j) {
		for (uint8_t i = 0; i < MAT_WIDTH; ++i) {
			if (j == i) {
				a[j][i] = 1.0f;
			}
			else {
				a[j][i] = 0.0f;
			}
		}
	}
}

void all(float a[MAT_HEIGHT][MAT_WIDTH], float val) {
	for (uint8_t j = 0; j < MAT_HEIGHT; ++j) {
		for (uint8_t i = 0; i < MAT_WIDTH; ++i) {
			a[j][i] = val;
		}
	}
}

int main(int argc, char* argv[]) {

	AppInfo appinfo;

	CpuInfo cpuinfo;

	printf("ParallelTest\n%s\n%s\n", appinfo.GetInfoStr().c_str(), cpuinfo.GetInfoStr().c_str());

	// 8x8flaot行列積。これはAVX2の効果あり
	float a[MAT_HEIGHT][MAT_WIDTH] = { 0.0f };
	float b[MAT_HEIGHT][MAT_WIDTH] = { 0.0f };
	float c[MAT_HEIGHT][MAT_WIDTH] = { 0.0f };
#if 1
	all(a,1);
	all(b,1);
#else
	tanni(a);
	tanni(b);
#endif
	Clock clock;
	clock.Begin();
	for (int i = 0; i < 100; ++i) {
		mul(a, b, c);
	}
	printf("Normal %lld[us]\n", clock.EndMicroSec());
	print(c);

	clock.Begin();
	for (int i = 0; i < 100; ++i) {
		mulavx2(a, b, c);
	}
	printf("AVX2 %lld[us]\n", clock.EndMicroSec());
	print(c);

	return 0;

	static const int CH = 4;
	static const int W = 1920;
	static const int H = 1080;
	static const int PIX = CH * W * H;

	const int16_t PARA = 4;
	const int16_t gain_nu = 2, gain_de = 10;
	const int16_t offset = 10;
	const __m256i gain_nu256 = _mm256_set1_epi16(gain_nu);
	const __m256i gain_de256 = _mm256_set1_epi16(gain_de);
	const __m256i offset256 = _mm256_set1_epi16(offset);

	uint8_t* pixels = new uint8_t[PIX];

	std::vector<long long> times;

	static const int TEST = 100;
	for (int t = 0; t < TEST; ++t) {
		memset(pixels, 128 + t, sizeof(uint8_t) * CH * W * H);
		printf("%d, %d\n", pixels[0], pixels[PIX - 1]);
		{
			Clock clock;
			clock.Begin();
			for (int y = 0; y < H; ++y) {
				for (int x = 0; x < (W/PARA); ++x) {
					uint8_t* p = &pixels[y * (W / PARA) * CH * PARA + x * CH * PARA];
#if 0
					// ゲインかけてオフセット。gainを分子分母に分けてfloat演算除去
					p[0] = CLIP((p[0] * gain_nu) / gain_de + offset);
					p[1] = CLIP((p[1] * gain_nu) / gain_de + offset);
					p[2] = CLIP((p[2] * gain_nu) / gain_de + offset);
					p[3] = CLIP((p[3] * gain_nu) / gain_de + offset);

					p[4] = CLIP((p[4] * gain_nu) / gain_de + offset);
					p[5] = CLIP((p[5] * gain_nu) / gain_de + offset);
					p[6] = CLIP((p[6] * gain_nu) / gain_de + offset);
					p[7] = CLIP((p[7] * gain_nu) / gain_de + offset);

					p[8] = CLIP((p[8] * gain_nu) / gain_de + offset);
					p[9] = CLIP((p[9] * gain_nu) / gain_de + offset);
					p[10] = CLIP((p[10] * gain_nu) / gain_de + offset);
					p[11] = CLIP((p[11] * gain_nu) / gain_de + offset);

					p[12] = CLIP((p[12] * gain_nu) / gain_de + offset);
					p[13] = CLIP((p[13] * gain_nu) / gain_de + offset);
					p[14] = CLIP((p[14] * gain_nu) / gain_de + offset);
					p[15] = CLIP((p[15] * gain_nu) / gain_de + offset);
#else
					// 上記のAVX2実装。これでもまだ遅い
					__m256i p256 = _mm256_set_epi16(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);		
					__m256i ans = _mm256_adds_epi16(_mm256_div_epi16(_mm256_mullo_epi16(p256, gain_nu256), gain_de256), offset256);

					p[0] = CLIP(ans.m256i_i16[0]);
					p[1] = CLIP(ans.m256i_i16[1]);
					p[2] = CLIP(ans.m256i_i16[2]);
					p[3] = CLIP(ans.m256i_i16[3]);
					p[4] = CLIP(ans.m256i_i16[4]);
					p[5] = CLIP(ans.m256i_i16[5]);
					p[6] = CLIP(ans.m256i_i16[6]);
					p[7] = CLIP(ans.m256i_i16[7]);
					p[8] = CLIP(ans.m256i_i16[8]);
					p[9] = CLIP(ans.m256i_i16[9]);
					p[10] = CLIP(ans.m256i_i16[10]);
					p[11] = CLIP(ans.m256i_i16[11]);
					p[12] = CLIP(ans.m256i_i16[12]);
					p[13] = CLIP(ans.m256i_i16[13]);
					p[14] = CLIP(ans.m256i_i16[14]);
					p[15] = CLIP(ans.m256i_i16[15]);
#endif
				}
			}
			times.push_back(clock.EndMicroSec());
		}
		printf("%d, %d\n", pixels[0], pixels[PIX - 1]);
	}
	
	long long sumtime = 0;
	for (size_t i = 0; i < times.size(); ++i) {
		sumtime += times[i];
		printf("%lld [us]\n", times[i]);
	}
	printf("Average %lld [us]\n", sumtime / times.size());
	std::sort(times.begin(), times.end());
	printf("Center  %lld [us]\n", times[times.size()/2]);

	delete [] pixels;

	return 0;
}