#ifndef FFMPEG_AAC_ENCODER_H
#define FFMPEG_AAC_ENCODER_H

#include <vector>
#include <string>
#include <atomic>
#include "base-encoder-i.h"
#include "core-audio-data.h"

extern "C" {
#include "libavformat/avformat.h"
}

class FFMpegAACEncoder : public IBaseEncoder
{
public:
	FFMpegAACEncoder();
	virtual ~FFMpegAACEncoder();
	virtual bool Init();
	virtual bool Update(const char* jsondata);
	virtual void InputRawData(uint8_t** data, size_t* lineSize, int plane, uint64_t pts, bool* receiveFrame);
	virtual void GetEncodedData(BaseEncoder::EncodedFrameData* data);
	virtual void GetEncoderExtraData(uint8_t** data, size_t* size);

private:
	bool initialize_codec();

private:
	AVCodec* codec = nullptr;
	AVCodecContext* context = nullptr;

	uint8_t* samples[MAX_AV_PLANES] = { 0 };
	AVFrame* aframe = nullptr;
	int64_t total_samples = 0;

	std::vector<uint8_t> packet_buffer;

	size_t audio_planes = 0;
	size_t audio_size = 0;

	int frame_size = 0; 
	int frame_size_bytes = 0;

	std::atomic<bool> init_flag_;
	BaseEncoder::EncodedFrameData encoded_data_;

	uint64_t dts_offset_ = 0;
	uint64_t encoded_cnt_ = 0;
};

#endif