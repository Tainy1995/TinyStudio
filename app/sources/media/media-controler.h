#ifndef MEDIA_CONTROLER_H
#define MEDIA_CONTROLER_H

#include <thread>
#include <string>
#include <atomic>
#include <functional>
#include "circlebuf.h"
#include "wasapi-audio-monitor.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include "libswresample/swresample.h"
}

using MediaStopped = std::function<void()>;

struct FrameTimestamp
{
	int64_t frame_pts = 0;
	int64_t next_pts = 0;
	int64_t last_duration = 0;
};

class MediaControler 
{
public:
	MediaControler(MediaStopped cb);
	~MediaControler();
	bool StartMedia(const wchar_t* filepath);
	bool GetFrameReady();
	void GetFrameData(uint8_t** pic, int* lineSize, int& width, int& height);
	void RenderFrameFinish();
	void RenderAudio();

private:
	void MediaThreadImpl();
	bool InitAvFormatContext();
	bool InitDecoder();
	bool InitAudioDecoder();
	bool ReadFromFile(bool bVideo);
	bool DecodeNext(bool& get_frame);
	bool DecodeNextAudio(bool& get_frame);
	bool InitScaling();
	bool InitAudioScaling();
	bool ScaleVideoFrame();
	void CalcFramePts();
	void CalcAudioFramePts();
	int64_t GetEstimatedDuration(int64_t last_pts,bool bAudio);

private:

	MediaStopped media_stopped_cb_;

	std::thread media_thread_;
	std::wstring media_file_name_;

	std::atomic<bool> media_started_;
	std::atomic<bool> frame_ready_;
	std::atomic<bool> audio_ready_;

	AVFormatContext* av_format_context_ = nullptr;
	AVCodec* av_codec_ = nullptr;
	AVCodecContext* decoder_ = nullptr;
	AVFrame* in_frame = nullptr;
	AVStream* video_stream_ = nullptr;

	AVCodec* audio_av_codec_ = nullptr;
	AVCodecContext* audio_decoder_ = nullptr;
	AVFrame* audio_in_frame = nullptr;
	AVStream* audio_stream_ = nullptr;

	SwsContext* swscale_context_ = nullptr;
	int scale_linesizes[4] = { 0 };
	uint8_t* scale_pic[4] = { nullptr };
	int64_t start_pts_ = 0;

	FrameTimestamp video_time_;
	FrameTimestamp audio_time_;

	struct circlebuf video_packets_;
	struct circlebuf audio_packets_;

	std::atomic<int> video_cnt_;
	std::atomic<int> audio_cnt_;

	WasapiAudioMonitor* audio_monitor_ = nullptr;
};


#endif