#include <stdio.h>
#include <vector>
#include <algorithm>

#include "AppInfo.h"
#include "CpuInfo.h"
#include "Clock.h"

#define CLIP(v) static_cast<uint8_t>((v<0?0:(255<v?255:v)))

int main(int argc, char* argv[]) {

	AppInfo appinfo;

	CpuInfo cpuinfo;

	printf("ParallelTest\n%s\n%s\n", appinfo.GetInfoStr().c_str(), cpuinfo.GetInfoStr().c_str());

	static const int CH = 4;
	static const int W = 1920;
	static const int H = 1080;
	static const int PIX = CH * W * H;

	const float gain = 0.2f;
	const int32_t offset = 10;

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
					p[0] = CLIP(p[0] * gain + offset);
					p[1] = CLIP(p[1] * gain + offset);
					p[2] = CLIP(p[2] * gain + offset);
					p[3] = CLIP(p[3] * gain + offset);
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