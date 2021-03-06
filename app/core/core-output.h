#ifndef CORE_OUTPUT_H
#define CORE_OUTPUT_H

#include "core-component-i.h"
#include "core-video-data.h"
#include "core-audio-data.h"
#include "base-encoder-i.h"
#include "base-output-i.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <list>
#include <atomic>
#include <memory>


class CoreOutput : public ICoreComponent
{
public:
	CoreOutput();
	virtual ~CoreOutput();

	void PreEndup();
	void StartupCoreOutput();

private:
	void InitImpl();
	void VideoImpl();
	void AudioImpl();
	void VideoRawDataReceived(const CoreVideoData::RawData* data);
	void AudioRawDataReceived(const CoreAudioData::AudioMixerOutput* data);
	void CoverBgraToNeededFormat(uint8_t* input_data, size_t input_linesize,
		uint8_t** output_data, size_t* output_linesize, int* plane, int width, int height);
	void DeepCopyEncodedData(BaseEncoder::EncodedFrameData* dst, const BaseEncoder::EncodedFrameData* src);
	void InitData();

private:
	std::thread init_thread_;
	std::thread video_thread_;
	std::thread audio_thread_;
	std::atomic<bool> final_exit_;

	std::mutex video_raw_data_mutex_;
	std::list<CoreVideoData::RawData> video_raw_data_list_;
	std::condition_variable video_raw_data_cond_;

	std::mutex audio_raw_data_mutex_;
	std::list<CoreAudioData::AudioMixerOutput> audio_raw_data_list_;
	std::condition_variable audio_raw_data_cond_;

	std::mutex encoded_data_mutex_;
	std::list< BaseEncoder::EncodedFrameData> encoded_video_list_;
	std::list< BaseEncoder::EncodedFrameData > encoded_audio_list_;
	uint64_t encoded_video_offset_ = 0;
	uint64_t encoded_audio_offset_ = 0;
	int64_t video_highest_ts_ = 0;
	int64_t audio_highest_ts_ = 0;
	int64_t video_stopped_ts_ = 0;
	int64_t audio_stopped_ts_ = 0;
	bool encoded_video_received_ = false;
	bool encoded_audio_received_ = false;
	bool encoded_video_send_ = false;

	std::shared_ptr<IBaseEncoder> video_encoder_;
	std::shared_ptr<IBaseEncoder> audio_encoder_;
	std::shared_ptr<IBaseOutput> output_instance_;

	uint64_t output_start_time_ = 0;
};

#endif