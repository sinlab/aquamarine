#pragma once

#include <chrono>

class Clock {
	// コンストラクタ～デストラクタ間の時間計測
private:
	std::chrono::system_clock::time_point mBegin, mEnd;
	long long* mTimePtr;
	long long DurationMicroSec(const std::chrono::system_clock::time_point& begin, const std::chrono::system_clock::time_point& end) const {
		return std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
	}
	long long DurationMilliSec(const std::chrono::system_clock::time_point& begin, const std::chrono::system_clock::time_point& end) const {
		return std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
	}
public:
	Clock(long long* timePtr = nullptr)
		: mBegin(std::chrono::system_clock::now()), mTimePtr(timePtr) {}
	~Clock() {
		mEnd = std::chrono::system_clock::now();
		if (mTimePtr != nullptr) {
			*mTimePtr = DurationMilliSec(mBegin, mEnd);
		}
	}
	void Begin() {
		mBegin = std::chrono::system_clock::now();
	}
	long long EndMilliSec() {
		mEnd = std::chrono::system_clock::now();
		return DurationMilliSec(mBegin, mEnd);
	}
	long long EndMicroSec() {
		mEnd = std::chrono::system_clock::now();
		return DurationMicroSec(mBegin, mEnd);
	}
};