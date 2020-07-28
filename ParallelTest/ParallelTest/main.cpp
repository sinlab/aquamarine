#include <stdio.h>
#include <vector>
#include <algorithm>

#include "AppInfo.h"
#include "CpuInfo.h"
#include "Clock.h"

#define CLIP(v) (v)//((v<0?0:(255<v?255:v)))

#include <xmmintrin.h>	// SSE
//#include <immintrin.h>	// AVX2

int main(int argc, char* argv[]) {

	AppInfo appinfo;

	CpuInfo cpuinfo;

	printf("ParallelTest\n%s\n%s\n", appinfo.GetInfoStr().c_str(), cpuinfo.GetInfoStr().c_str());

	static const int CH = 4;
	static const int W = 1920;
	static const int H = 1080;
	static const int PIX = CH * W * H;

	const int32_t gain_nu = 2, gain_de = 10;
	const float gain = (float)gain_nu / (float)gain_de;
	const int32_t offset = 10;
	const __m128i gain_nu128 = _mm_setr_epi32(gain_nu, gain_nu, gain_nu, gain_nu);
	const __m128i gain_de128 = _mm_setr_epi32(gain_de, gain_de, gain_de, gain_de);
	const __m128i offset128 = _mm_setr_epi32(offset, offset, offset, offset);

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
				for (int x = 0; x < W; ++x) {
					uint8_t* p = &pixels[y * W * CH + x * CH];
#if 0
#if 0
					// ゲインかけてオフセット。素直な実装
					p[0] = CLIP(p[0] * gain + offset);
					p[1] = CLIP(p[1] * gain + offset);
					p[2] = CLIP(p[2] * gain + offset);
					p[3] = CLIP(p[3] * gain + offset);
#else
					// ゲインかけてオフセット。gainを分子分母に分けてfloat演算除去
					p[0] = CLIP((p[0] * gain_nu) / gain_de + offset);
					p[1] = CLIP((p[1] * gain_nu) / gain_de + offset);
					p[2] = CLIP((p[2] * gain_nu) / gain_de + offset);
					p[3] = CLIP((p[3] * gain_nu) / gain_de + offset);
#endif
#else
					// 上記をSSEで実装。128/32bitで4並列。遅くなってしまう
					__m128i ans =_mm_add_epi32(_mm_div_epi32(_mm_mullo_epi32(_mm_setr_epi32(p[0], p[1], p[2], p[3]), gain_nu128), gain_de128), offset128);
					p[0] = CLIP(ans.m128i_i32[0]);
					p[1] = CLIP(ans.m128i_i32[1]);
					p[2] = CLIP(ans.m128i_i32[2]);
					p[3] = CLIP(ans.m128i_i32[3]);
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