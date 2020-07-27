#include <stdio.h>

#include "AppInfo.h"
#include "CpuInfo.h"

int main(int argc, char* argv[]) {

	AppInfo appinfo;

	CpuInfo cpuinfo;

	printf("ParallelTest\n%s\n%s\n", appinfo.GetInfoStr().c_str(), cpuinfo.GetInfoStr().c_str());

	return 0;
}