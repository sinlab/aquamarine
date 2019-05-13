#include "../ocvwrapper/opencvwrapper.h"

#include <stdio.h>

int main() {

	/*
	unsigned char bgr[] = { 0,0,255,0,255,0,255,0,0 };
	unsigned char gray[] = { 255,0,0 };

	auto handle = Open(3, 1, PLANE_TYPE::BGR, bgr);
	*/

	auto handle = Open(320, 160, PLANE_TYPE::BGR);

	WriteRect(&handle, { 0,0,320,160 }, { 255,0,0 }, 1);

	Save(&handle, "hoge.png");

	Close(&handle);

}