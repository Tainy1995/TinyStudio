#ifndef WASAPI_AUDIO_MONITOR_H
#define WASAPI_AUDIO_MONITOR_H

/*wasapi的音频监控
* source如果含有音频，注册一个monitor并把数据通过monitor调用到wasapi去播放
*/

#include <vector>
#include <string>
#include <functional>
#include <Audioclient.h>
#include <synchapi.h>
#include <vector>
#include "core-audio-data.h"
#include "audio-resampler.h"

extern "C" {
#include "circlebuf.h"
}

class WasapiAudioMonitor
{
public:
	WasapiAudioMonitor();
	~WasapiAudioMonitor();
	bool CreateWasapiMonitor(const CoreAudioData::resample_info* input_info);
	void InputAudioData(const uint8_t* const input[], uint32_t in_frames, uint64_t timestamp);
	void UpdateLastVideoFrameTime(uint64_t ts) { last_frame_ts_ = ts; }

private:
	bool CreateWasapiImpl();
	void FreeContextForReconnect();
	bool ProcessAudioDelay(float** data, uint32_t* frames, uint64_t ts, uint32_t pad);

private:
	AudioResampler* audio_resample_ = nullptr;
	IAudioClient* audio_client_ = nullptr;
	IAudioRenderClient* audio_render_ = nullptr;
	uint64_t last_recv_time_ = 0;
	uint64_t prev_video_ts_ = 0;
	uint64_t time_since_prev_ = 0;
	uint64_t last_frame_ts_ = 0;
	uint32_t sample_rate_ = 0;
	uint32_t channels_ = 0;
	int64_t lowest_audio_offset_ = 0;
	struct circlebuf delay_buffer;
	uint32_t delay_size_ = 0;
	std::vector<float> buf_;
	SRWLOCK playback_mutex_;
	CoreAudioData::resample_info input_info_;
	bool call_once_init_ = false;
	bool com_initialized_ = false;
};

#endif