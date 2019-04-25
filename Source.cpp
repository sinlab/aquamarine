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
