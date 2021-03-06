#ifndef CORE_AUDIO_H
#define CORE_AUDIO_H

#include <thread>
#include <atomic>
#include <functional>
#include "core-component-i.h"
#include "audio-resampler.h"
#include "wasapi-audio-source.h"
#include "circlebuf.h"

using CoreAudioDataCallback = std::function<void(const CoreAudioData::AudioMixerOutput* data)>;

class CoreAudio : public ICoreComponent
{
public:
	CoreAudio();
	virtual ~CoreAudio();
	void PreEndup();

	void StartupCoreAudio();
	void RegisterCoreAudioDataCallback(void* ptr, CoreAudioDataCallback cb);
	void UnRegisterCoreAudioDataCallback(void* ptr);
	uint64_t GetCaptureStartTs() { return audio_capture_start_ts_.load(); }

private:
	void AudioThreadImpl();
	void DesktopAudioDataReceived(const CoreAudioData::AudioMixerOutput* output);
	void OutputCoreAudioData(uint64_t timestamp);

private:
	std::thread audio_thread_;
	std::atomic<bool> audio_run_;
	std::atomic<uint64_t> audio_capture_start_ts_;
	AudioResampler* audio_resampler_ = nullptr;
	WasapiAudioSource* desktop_audio_ = nullptr;
	struct circlebuf desktop_audio_buf_[MAX_AUDIO_CHANNELS];
	std::mutex desktop_audio_mutex_;
	std::mutex audio_callback_mutex_;
	std::map< void*, CoreAudioDataCallback> audio_callback_map_;
};

#endif