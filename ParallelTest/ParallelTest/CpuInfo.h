#pragma once
#include <intrin.h>	// __cpuid, __cpuidex
#include <string>

class CpuInfo {
private:
	std::string mCpuName;
	bool mMMX;
	bool mSSE;
	bool mSSE2;
	bool mSSE3;
	bool mSSSE3;
	bool mSSE41;
	bool mSSE42;
	bool mAVX;
	bool mAVX2;
	bool mAVX512F;

	enum Operations {
		// CPUID 1 ECX
		SSE3_READY		= (1 << 0),
		SSSE3_READY		= (1 << 9),
		SSE41_READY		= (1 << 19),
		SSE42_READY		= (1 << 20),
		AVX_READY		= (1 << 28),
		// CPUID 1 EDX
		MMX_READY		= (1 << 23),
		SSE_READY		= (1 << 25),
		SSE2_READY		= (1 << 26),
		// CPUID 7 0 EBX
		AVX2_READY		= (1 << 5),
		AVX512F_READY	= (1 << 16)
	};

	std::string InitCpuName() const {
		// CPU情報（ベンダー、型番、動作周波数）取得
		int cpuInfo[4] = { 0 };
		__cpuid(cpuInfo, 0x80000000);
		std::string name;
		if (0x80000002 <= cpuInfo[0]) {
			int cpuInfoV[3][4];
			__cpuid(cpuInfoV[0], 0x80000002);
			__cpuid(cpuInfoV[1], 0x80000003);
			__cpuid(cpuInfoV[2], 0x80000004);
			const char* p = (char*)cpuInfoV;
			char temp[4 * 4 * 4] = { 0 };
			for (int n = 0; n < 4 * 4 * 3; n++) {
				temp[n] = *p++;
			}
			name = temp;
		}
		return name;
	}

	void InitCpuSimdExtension() {
		// 拡張命令対応情報取得
		int cpuInfo[4] = { 0 };
		__cpuid(cpuInfo, 0x00000000);
		const int idMax = cpuInfo[0];	// EAX
		if (1 <= idMax) {
			__cpuid(cpuInfo, 0x00000001);
			// CPUID 1 ECX
			mSSE3 = cpuInfo[2] & SSE3_READY;
			mSSSE3 = cpuInfo[2] & SSSE3_READY;
			mSSE41 = cpuInfo[2] & SSE41_READY;
			mSSE42 = cpuInfo[2] & SSE42_READY;
			mAVX = cpuInfo[2] & AVX_READY;
			// CPUID 1 EDX
			mMMX = cpuInfo[3] & MMX_READY;
			mSSE = cpuInfo[3] & SSE_READY;
			mSSE2 = cpuInfo[3] & SSE2_READY;
		}
		if (7 <= idMax) {
			__cpuidex(cpuInfo, 0x00000007, 0x00000000);
			// CPUID 7 0 EBX
			mAVX2 = cpuInfo[1] & AVX2_READY;
			mAVX512F = cpuInfo[1] & AVX512F_READY;
		}
	 }

public:
	CpuInfo() {
		// https://docs.microsoft.com/ja-jp/cpp/intrinsics/cpuid-cpuidex?view=vs-2019
		// http://yamatyuu.net/computer/program/vc2013/extinstchk/index.html

		mCpuName = InitCpuName();

		InitCpuSimdExtension();		
	}

	std::string GetCpuName() const { return mCpuName; }
	bool HasMMX() const { return mMMX; }
	bool HasSSE() const { return mSSE; }
	bool HasSSE2() const { return mSSE2; }
	bool HasSSE3() const { return mSSE3; }
	bool HasSSSE3() const { return mSSSE3; }
	bool HasSSE41() const { return mSSE41; }
	bool HasSSE42() const { return mSSE42; }
	bool HasAVX() const { return mAVX; }
	bool HasAVX2() const { return mAVX2; }
	bool HasAVX512() const { return mAVX512F; }
};