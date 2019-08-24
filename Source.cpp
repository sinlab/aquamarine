#define __STDC_CONSTANT_MACROS
#define __STDC_LIMIT_MACROS
#include <stdio.h>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "swresample.lib")

#include <string>
#include <vector>

class FFmpegWrapper {
protected:
	AVFormatContext* mFormatContext;
	AVStream* mStream;
	AVCodecContext* mCodecContext;
	AVCodec* mCodec;
	AVFrame* mFrame;
	bool mIsOpen;
	bool mFlushed;
	bool mHasFrame;

	virtual AVMediaType TargetMediaType() const = 0;
	virtual bool OnFrameDecoded(AVFrame* frame) = 0;

	bool ReadNext() {
		AVPacket packet = AVPacket();

		while (av_read_frame(mFormatContext, &packet) == 0) {
			if (packet.stream_index == mStream->index) {
				if (avcodec_send_packet(mCodecContext, &packet) != 0) {
					printf("avcodec_send_packet failed\n");
				}
				while (avcodec_receive_frame(mCodecContext, mFrame) == 0) {
					return OnFrameDecoded(mFrame);
				}
			}
			av_packet_unref(&packet);
		}

		if (!mFlushed) {
			// flush decoder
			if (avcodec_send_packet(mCodecContext, nullptr) != 0) {
				printf("avcodec_send_packet failed");
			}
			while (avcodec_receive_frame(mCodecContext, mFrame) == 0) {
				mFlushed = true;
				return OnFrameDecoded(mFrame);
			}
		}

		return false;
	}

public:
	FFmpegWrapper()
		: mFormatContext(nullptr), mStream(nullptr), mCodecContext(nullptr), mCodec(nullptr), mFrame(nullptr)
		, mIsOpen(false), mFlushed(false), mHasFrame(false) {}
	virtual ~FFmpegWrapper() { Close(); }

	bool Open(const std::string& path) {

		if (mIsOpen) { return false; }

		if (avformat_open_input(&mFormatContext, path.c_str(), nullptr, nullptr) != 0) {
			Close();
			return false;
		}

		if (avformat_find_stream_info(mFormatContext, nullptr) < 0) {
			printf("avformat_find_stream_info failed\n");
		}

		for (int i = 0; i < (int)mFormatContext->nb_streams; ++i) {
			if (mFormatContext->streams[i]->codecpar->codec_type == TargetMediaType()) {
				mStream = mFormatContext->streams[i];
				break;
			}
		}
		if (mStream == nullptr) {
			Close();
			return false;
		}

		AVCodec* codec = avcodec_find_decoder(mStream->codecpar->codec_id);
		if (codec == nullptr) {
			Close();
			return false;
		}

		mCodecContext = avcodec_alloc_context3(codec);
		if (mCodecContext == nullptr) {
			Close();
			return false;
		}

		if (avcodec_parameters_to_context(mCodecContext, mStream->codecpar) < 0) {
			Close();
			return false;
		}

		if (avcodec_open2(mCodecContext, codec, nullptr) != 0) {
			Close();
			return false;
		}

		mFrame = av_frame_alloc();

		mIsOpen = true;
		mHasFrame = ReadNext();

		return true;
	}

	virtual void Close() {
		if (mFrame) {
			av_frame_free(&mFrame);
			mFrame = nullptr;
		}
		if (mCodecContext) {
			avcodec_free_context(&mCodecContext);
			mCodecContext = nullptr;
		}
		if (mFormatContext) {
			avformat_close_input(&mFormatContext);
			mFormatContext = nullptr;
			mStream = nullptr;
		}

		mIsOpen = false;
		mFlushed = false;
		mHasFrame = false;
	}
};

class FFmpegVideoDecoder : public FFmpegWrapper {
public:
	FFmpegVideoDecoder() : FFmpegWrapper() {}
	virtual ~FFmpegVideoDecoder() = default;

	bool GetFrame(std::vector<unsigned char>& y, std::vector<unsigned char>& u, std::vector<unsigned char>& v) {
		if (mHasFrame) {
			y = mY;
			u = mU;
			v = mV;
			mHasFrame = ReadNext();
			return true;
		} else { return false; }
	}

	void Close() {
		FFmpegWrapper::Close();
		mY.clear();
		mU.clear();
		mV.clear();
	}

protected:
	AVMediaType TargetMediaType() const override { return AVMEDIA_TYPE_VIDEO; }

	bool OnFrameDecoded(AVFrame* frame) override {

		if (!(frame->format == AV_PIX_FMT_YUV420P || frame->format == AV_PIX_FMT_YUVJ420P)) {
			return false;
		}

		const int pix = Width() * Height();

		if (pix != mY.size()) {
			mY.resize(pix);
			mU.resize(pix / 2 / 2);
			mV.resize(pix / 2 / 2);
		}

		for (int h = 0; h < Height(); ++h) {
			memcpy(mY.data() + h * Width(), frame->data[0] + h * frame->linesize[0], Width());
		}

		for (int h = 0; h < Height()/2; ++h) {
			memcpy(mU.data() + h * Width() / 2, frame->data[1] + h * frame->linesize[1], Width() / 2);
			memcpy(mV.data() + h * Width() / 2, frame->data[2] + h * frame->linesize[2], Width() / 2);
		}

		return true;
	}

	int Width() const {
		if (mIsOpen) {
			return mCodecContext->width;
		}
		else { return -1; }
	}
	int Height() const {
		if (mIsOpen) {
			return mCodecContext->height;
		}
		else { return -1; }
	}
	double FrameRate() const {
		if (mIsOpen) {
			auto rate = mCodecContext->framerate;
			return (double)rate.num / rate.den;
		}
		else { return -1; }
	}

	std::vector<unsigned char> mY, mU, mV;
};

class FFmpegAudioDecoder : public FFmpegWrapper {
public:
	FFmpegAudioDecoder() : FFmpegWrapper(), mSwResample(nullptr) {}
	virtual ~FFmpegAudioDecoder() = default;

	void Close() override {
		FFmpegWrapper::Close();
		if (mSwResample) {
			swr_free(&mSwResample);
			mSwResample = nullptr;
		}
		mPcm.clear();
	}

	bool GetFrame(std::vector<short>& pcm) {
		if (mHasFrame) {
			pcm = mPcm;
			mHasFrame = ReadNext();
			return true;
		}
		else { return false; }
	}

protected:
	AVMediaType TargetMediaType() const override { return AVMEDIA_TYPE_AUDIO; }

	bool OnFrameDecoded(AVFrame* frame) override {
		
		if (!(frame->format == AV_SAMPLE_FMT_FLTP)) {
			return false;
		}

		if (mSwResample == nullptr) {
			mSwResample = swr_alloc();
			av_opt_set_int(mSwResample, "in_channel_layout", frame->channel_layout, 0);
			av_opt_set_int(mSwResample, "out_channel_layout", frame->channel_layout, 0);
			av_opt_set_int(mSwResample, "in_sample_rate", frame->sample_rate, 0);
			av_opt_set_int(mSwResample, "out_sample_rate", frame->sample_rate, 0);
			av_opt_set_sample_fmt(mSwResample, "in_sample_fmt", (AVSampleFormat)frame->format, 0);
			av_opt_set_sample_fmt(mSwResample, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
			if (swr_init(mSwResample) < 0) {
				return false;
			}
			mPcm.resize(frame->nb_samples * frame->channels);
		}

		short *pData = mPcm.data();
		if (swr_convert(mSwResample, (uint8_t**)(&pData), frame->nb_samples, (const uint8_t**)frame->extended_data, frame->nb_samples) < 0) {
			return false;
		}

		return true;
	}

	int Channels() const {
		if (mIsOpen) {
			return mCodecContext->channels;
		}
		else { return -1; }
	}
	int SampleRate() const {
		if (mIsOpen) {
			return mCodecContext->sample_rate;
		}
		else { return -1; }
	}

	SwrContext *mSwResample;
	std::vector<short> mPcm;
};

int main() {
	
	std::vector<unsigned char> y, u, v;

	FFmpegVideoDecoder vd;

	vd.Open("gupsta.mp4");

	while (vd.GetFrame(y,u,v)) {
		FILE *fp;
		fopen_s(&fp, "y.yuv", "wb");
		fwrite(y.data(), 1, y.size(), fp);
		fclose(fp);
		fopen_s(&fp, "u.yuv", "wb");
		fwrite(u.data(), 1, u.size(), fp);
		fclose(fp);
		fopen_s(&fp, "v.yuv", "wb");
		fwrite(v.data(), 1, v.size(), fp);
		fclose(fp);
	}

	vd.Close();

	FFmpegAudioDecoder a;

	a.Open("gupsta.mp4");

	std::vector<short> fltp;
	FILE *fp;
	fopen_s(&fp, "fltp.pcm", "wb");
	while (a.GetFrame(fltp)) {
		fwrite(fltp.data(), sizeof(short), fltp.size(), fp);
	}
	fclose(fp);

	a.Close();

	a.Open("gupsta.mp4");

	fopen_s(&fp, "fltp2.pcm", "wb");
	while (a.GetFrame(fltp)) {
		fwrite(fltp.data(), sizeof(short), fltp.size(), fp);
	}
	fclose(fp);

	a.Close();
}

#if 0

#define __STDC_CONSTANT_MACROS
#define __STDC_LIMIT_MACROS
#include <stdio.h>
#include <deque>
#include <vector>
extern "C" {
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

typedef struct {
	std::vector<unsigned char> y, u, v;
} YUV420;

std::deque<YUV420> yuvs;

static void on_frame_decoded(AVFrame* frame) {
	AVFrame* new_ref = av_frame_alloc();
	av_frame_ref(new_ref, frame);

	YUV420 yuv;
	int w = new_ref->width;
	int h = new_ref->height;
	yuv.y.resize(w*h);
	yuv.u.resize(w*h / 2 / 2);
	yuv.v.resize(w*h / 2 / 2);

	for (int y = 0; y < h; ++y) {
		memcpy(yuv.y.data() + w * y, new_ref->data[0] + y * new_ref->linesize[0], w);
	}

	for (int y = 0; y < h / 2; ++y) {
		memcpy(yuv.u.data() + w / 2 * y, new_ref->data[1] + y * new_ref->linesize[1], w / 2);
		memcpy(yuv.v.data() + w / 2 * y, new_ref->data[2] + y * new_ref->linesize[2], w / 2);
	}

	yuvs.push_back(yuv);
}

void decode_all()
{
	const char* input_path = "gupsta.mp4";
	AVFormatContext* format_context = nullptr;
	if (avformat_open_input(&format_context, input_path, nullptr, nullptr) != 0) {
		printf("avformat_open_input failed\n");
	}

	if (avformat_find_stream_info(format_context, nullptr) < 0) {
		printf("avformat_find_stream_info failed\n");
	}

	AVStream* video_stream = nullptr;
	for (int i = 0; i < (int)format_context->nb_streams; ++i) {
		if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_stream = format_context->streams[i];
			break;
		}
	}
	if (video_stream == nullptr) {
		printf("No video stream ...\n");
	}

	AVCodec* codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
	if (codec == nullptr) {
		printf("No supported decoder ...\n");
	}

	AVCodecContext* codec_context = avcodec_alloc_context3(codec);
	if (codec_context == nullptr) {
		printf("avcodec_alloc_context3 failed\n");
	}

	if (avcodec_parameters_to_context(codec_context, video_stream->codecpar) < 0) {
		printf("avcodec_parameters_to_context failed\n");
	}

	if (avcodec_open2(codec_context, codec, nullptr) != 0) {
		printf("avcodec_open2 failed\n");
	}

	AVFrame* frame = av_frame_alloc();
	AVPacket packet = AVPacket();

	while (av_read_frame(format_context, &packet) == 0) {
		if (packet.stream_index == video_stream->index) {
			if (avcodec_send_packet(codec_context, &packet) != 0) {
				printf("avcodec_send_packet failed\n");
			}
			while (avcodec_receive_frame(codec_context, frame) == 0) {
				on_frame_decoded(frame);
			}
		}
		av_packet_unref(&packet);
	}

	// flush decoder
	if (avcodec_send_packet(codec_context, nullptr) != 0) {
		printf("avcodec_send_packet failed");
	}
	while (avcodec_receive_frame(codec_context, frame) == 0) {
		on_frame_decoded(frame);
	}

	av_frame_free(&frame);
	avcodec_free_context(&codec_context);
	avformat_close_input(&format_context);
}

class Encoder {
private:
	bool mIsOpen;
	std::string mOutPath;
	AVIOContext* mIoContext;
	AVFormatContext* mFormatContext;
	AVCodecContext* mCodecContext;
	AVStream* mStream;
	AVFrame* mWriteFrame;
	const AVRational mTimebase;
	int mWidth;
	int mHeight;
	double mFps;
	int mFrameCount;

	void SendFrame(const AVFrame* frame) {
		if (avcodec_send_frame(mCodecContext, frame) != 0) {
			printf("avcodec_send_frame failed\n");
		}
		AVPacket packet = AVPacket();
		while (avcodec_receive_packet(mCodecContext, &packet) == 0) {
			packet.stream_index = 0;
			av_packet_rescale_ts(&packet, mCodecContext->time_base, mStream->time_base);
			if (av_interleaved_write_frame(mFormatContext, &packet) != 0) {
				printf("av_interleaved_write_frame failed\n");
			}
		}
	}

public:
	Encoder() : mIsOpen(false), mIoContext(nullptr), mFormatContext(nullptr), mCodecContext(nullptr), mStream(nullptr), mWriteFrame(nullptr), mTimebase({ 1, 1000000 }),
		mWidth(0), mHeight(0), mFps(0), mFrameCount(0) {}

	bool Open(const std::string& outpath, int w, int h, double fps) {
		mOutPath = outpath;
		mWidth = w;
		mHeight = h;
		mFps = fps;

		if (avio_open(&mIoContext, mOutPath.c_str(), AVIO_FLAG_WRITE) < 0) {
			printf("avio_open failed\n");
		}

		if (avformat_alloc_output_context2(&mFormatContext, nullptr, "mp4", nullptr) < 0) {
			printf("avformat_alloc_output_context2 failed\n");
		}

		mFormatContext->pb = mIoContext;

		AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
		if (codec == nullptr) {
			printf("encoder not found ...\n");
		}

		mCodecContext = avcodec_alloc_context3(codec);
		if (mCodecContext == nullptr) {
			printf("avcodec_alloc_context3 failed\n");
		}

		// set picture properties
		//codec_context->bit_rate = 400000;
		mCodecContext->width = mWidth;
		mCodecContext->height = mHeight;
		mCodecContext->time_base = mTimebase;
		mCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;

		// generate global header when the format require it
		if (mFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
			mCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		}

		// make codec options
		AVDictionary* codec_options = nullptr;
		av_dict_set(&codec_options, "preset", "medium", 0);
		av_dict_set(&codec_options, "crf", "22", 0);
		av_dict_set(&codec_options, "profile", "high", 0);
		av_dict_set(&codec_options, "level", "4.0", 0);

		if (avcodec_open2(mCodecContext, mCodecContext->codec, &codec_options) != 0) {
			printf("avcodec_open2 failed\n");
		}

		mStream = avformat_new_stream(mFormatContext, codec);
		if (mStream == NULL) {
			printf("avformat_new_stream failed");
		}

		mStream->sample_aspect_ratio = mCodecContext->sample_aspect_ratio;
		mStream->time_base = mCodecContext->time_base;

		if (avcodec_parameters_from_context(mStream->codecpar, mCodecContext) < 0) {
			printf("avcodec_parameters_from_context failed");
		}

		if (avformat_write_header(mFormatContext, nullptr) < 0) {
			printf("avformat_write_header failed\n");
		}

		mWriteFrame = av_frame_alloc();
		mWriteFrame->format = mCodecContext->pix_fmt;
		mWriteFrame->width = mCodecContext->width;
		mWriteFrame->height = mCodecContext->height;
		int ret = av_frame_get_buffer(mWriteFrame, 32);
		ret = av_frame_make_writable(mWriteFrame);

		mIsOpen = true;
		return true;
	}

	bool Write(const unsigned char* y, const unsigned char* u, const unsigned char* v) {
		if (!mIsOpen) { return false; }

		// Y
		for (int j = 0; j < mCodecContext->height; j++) {
			memcpy(mWriteFrame->data[0] + j * mWriteFrame->linesize[0], y + j * mCodecContext->width, mCodecContext->width);
		}

		/* Cb and Cr */
		for (int j = 0; j < mCodecContext->height / 2; j++) {
			memcpy(mWriteFrame->data[1] + j * mWriteFrame->linesize[1], u + j * mCodecContext->width / 2, mCodecContext->width / 2);
			memcpy(mWriteFrame->data[2] + j * mWriteFrame->linesize[2], v + j * mCodecContext->width / 2, mCodecContext->width / 2);
		}

		const int64_t pts = mTimebase.den / mFps * mFrameCount;
		mWriteFrame->pts = av_rescale_q(pts, mTimebase, mCodecContext->time_base);
		mWriteFrame->key_frame = 0;
		mWriteFrame->pict_type = AV_PICTURE_TYPE_NONE;

		SendFrame(mWriteFrame);

		mFrameCount++;

		return true;
	}

	void Close() {
		if (!mIsOpen) { return; }

		// flush encoder
		SendFrame(nullptr);

		if (av_write_trailer(mFormatContext) != 0) {
			printf("av_write_trailer failed\n");
		}

		av_frame_free(&mWriteFrame);
		avcodec_free_context(&mCodecContext);
		avformat_free_context(mFormatContext);
		avio_closep(&mIoContext);
	}
};


int main(int argc, char* argv[])
{
	decode_all();


	Encoder e;

	e.Open("output.mp4", 480, 270, 24);


	while (yuvs.size() > 0) {
		YUV420 yuv = yuvs.front();
		yuvs.pop_front();

		e.Write(yuv.y.data(), yuv.u.data(), yuv.v.data());
	}

	e.Close();

	return 0;
}

#endif