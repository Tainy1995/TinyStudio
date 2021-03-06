#ifndef FLVFILE_OUTPUT_H
#define FLVFILE_OUTPUT_H

#include <vector>
#include <string>
#include <mutex>
#include "circlebuf.h"
#include "base-output-i.h"
#include "core-audio-data.h"
extern "C" {
#include "libavformat/avformat.h"
}

class FileOutput : public IBaseOutput
{
public:
	FileOutput();
	virtual ~FileOutput();
	virtual bool Init();
	virtual bool Update(const char* json);
	virtual void SetVideoEncoder(std::shared_ptr<IBaseEncoder> encoder);
	virtual void SetAudioEncoder(std::shared_ptr<IBaseEncoder> encoder);
	virtual void InputEncodedData(BaseEncoder::EncodedFrameData encoded_data);

private:
	void StopOutputImpl();
	bool CreateVideoStream();
	bool AllocNewStream();
	bool CreateAudioStream();
	bool CreateNewAudioStream();
	bool OpenAudioCodec();
	bool AllocNewAvAudioStream(AVStream** stream, AVCodec** codec);
	bool OpenOutputFile();
	void SendOutputThreadImpl();
	void FreeCirclePackets();
	bool SendSPSPPS();
	int SendFFMpegPacket(struct BaseEncoder::EncodedFrameData* packet, bool is_header);
	int64_t RescaleTs(int64_t val, int idx);

private:
	std::mutex output_mutex_;
	bool output_released_ = true;
	bool output_running_ = false;
	bool output_start_flag_ = false;
	bool send_sps_pps_ = false;
	std::thread* output_run_thread_ = nullptr;
	std::condition_variable output_cond_;

	AVFormatContext* av_format_context_ = nullptr;
	AVCodec* av_video_codec_ = nullptr;
	AVStream* av_video_stream_ = nullptr;
	AVFrame* av_video_frame_ = nullptr;
	AVCodecContext* av_video_codec_context_ = nullptr;

	AVStream** av_audio_streams_ = nullptr;
	AVCodec* av_audio_codec_ = nullptr;
	AVFrame* av_audio_frame_[MAX_AUDIO_MIXES];
	AVCodecContext* av_audio_codec_context_ = nullptr;
	circlebuf av_packets_;
};

#endif