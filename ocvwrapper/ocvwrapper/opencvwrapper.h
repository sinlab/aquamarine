#pragma once

extern "C" {

#define DECLSPEC_DLLPORT	__declspec(dllexport)

	typedef enum {
		GRAYSCALE,
		BGR
	} PLANE_TYPE;

	typedef struct {
		int w;
		int h;
		PLANE_TYPE type;
		void *pData;
	} IMAGE_HANDLE;

	typedef struct {
		int b, g, r;
	} COLORBGR;
	typedef struct {
		int x, y, w, h;
	} RECT;

	DECLSPEC_DLLPORT IMAGE_HANDLE Open(int w, int h, PLANE_TYPE type, const unsigned char* pixels = nullptr);
	DECLSPEC_DLLPORT void Close(IMAGE_HANDLE *handle);

	DECLSPEC_DLLPORT void Save(const IMAGE_HANDLE *handle, const char* path);

	DECLSPEC_DLLPORT void WriteRect(IMAGE_HANDLE *handle, RECT rect, COLORBGR bgr, int thickness);
}
