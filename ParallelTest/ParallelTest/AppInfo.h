#pragma once

#include <string>

class AppInfo {
private:
	std::string mBuild;
	std::string mPlatform;
	std::string mRuntime;
	int mMscVerVal;
	std::string mMscVer;

public:
	AppInfo() {
#ifdef _DEBUG
		mBuild = "Debug";
#else
		mBuild = "Release";
#endif

#ifdef _WIN64
		mPlatform = "x64";
#else
		mPlatform = "x86";
#endif

#ifdef _DLL
		mRuntime = "Multi Thread DLL";
#else
		mRuntime = "Multi Thread";
#endif

		{
			// https://qiita.com/yumetodo/items/8c112fca0a8e6b47072d
			mMscVerVal = _MSC_VER;

			if (1920 <= mMscVerVal) {
				mMscVer = "VC++2019";
			}
			else if (1910 <= mMscVerVal) {
				mMscVer = "VC++2017";
			}
			else if (1900 <= mMscVerVal) {
				mMscVer = "VC++2015";
			}
			else if (1800 <= mMscVerVal) {
				mMscVer = "VC++2013";
			}
			else {
				mMscVer = "Older than 2013";
			}
		}
	}

	std::string GetInfoStr() const {
		return (mMscVer + ", " + mRuntime + ", " + mBuild + ", " + mPlatform);
	}
};