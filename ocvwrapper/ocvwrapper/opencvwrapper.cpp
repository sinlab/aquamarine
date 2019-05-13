#include "opencvwrapper.h"

#include <opencv2/opencv.hpp>


IMAGE_HANDLE Open(int w, int h, PLANE_TYPE type, const unsigned char* pixels) {

	IMAGE_HANDLE handle = { w, h, type, reinterpret_cast<void*>(new cv::Mat(h, w, CV_8UC3)) };
	auto mat = reinterpret_cast<cv::Mat*>((&handle)->pData);
	memset(mat->data, 0, sizeof(unsigned char) * (mat->rows) * (mat->cols) * 3);

	if (pixels) {
		if (handle.type == PLANE_TYPE::GRAYSCALE) {
			for (int i = 0; i < w*h; ++i) {
				mat->data[i * 3 + 0] = mat->data[i * 3 + 1] = mat->data[i * 3 + 2] = pixels[i];
			}
		}
		else if (handle.type == PLANE_TYPE::BGR) {
			memcpy(mat->data, pixels, sizeof(unsigned char) * (mat->rows) * (mat->cols) * 3);
		}
		else {
			// error!
		}
	}

	return handle;
}

void Close(IMAGE_HANDLE *handle) {
	if (handle == nullptr) { return; }
	auto pData = reinterpret_cast<cv::Mat*>(handle->pData);
	if (pData) { delete pData; }
	handle->w = 0;
	handle->h = 0;
	handle->type = PLANE_TYPE::GRAYSCALE;
	handle->pData = nullptr;
}

void Save(const IMAGE_HANDLE *handle, const char* path) {
	if (handle == nullptr) { return; }
	auto pData = reinterpret_cast<cv::Mat*>(handle->pData);
	if (pData) {
		cv::imwrite(path, *pData);
	}
}

void WriteRect(IMAGE_HANDLE *handle, RECT rect, COLORBGR bgr, int thickness) {
	if (handle == nullptr) { return; }
	auto pData = reinterpret_cast<cv::Mat*>(handle->pData);
	if (pData) {
		cv::rectangle(*pData, cv::Point2i(rect.x,rect.y), cv::Point2i(rect.x + rect.w - 1, rect.y + rect.h - 1), cv::Scalar(bgr.b, bgr.g, bgr.r), thickness);
	}
}
