#ifndef WASAPI_AUDIO_SOURCE_H
#define WASAPI_AUDIO_SOURCE_H

/*wasapi的音频源
* 可以用于采集音频输入设备（麦克风），音频输出的设备（桌面音频）的声音
* 基本wasapi采集音频设备的音频数据，outputmgr可以通过这个source来获取音频送去编码
* 
*/

#include <wrl/client.h>
#include <vector>
#include <string>
#include <functional>
#include <Windows.h>
#include <mmeapi.h>
#include <mmdeviceapi.h>
#include <list>
#include <mutex>
#include <condition_variable>
#include <Audioclient.h>
#include "dx-header.h"
#include "core-audio-data.h"
#include "WinHandle.hpp"
#include <mmdeviceapi.h>

using AudioDataReceived = std::function<void(const CoreAudioData::AudioMixerOutput*)>;

class WasapiAudioSource
{
	enum class TaskType
	{
		kTaskInitialize = 0,
		kTaskCaptureData,
		kTaskQuit
	};

public:
	WasapiAudioSource();
	~WasapiAudioSource();

	bool InitWasapiDevice(bool bInputDevice, bool bUseDefault, bool bUseDeviceTime, std::string sDeviceId = "");
	void RegisterAudioDataReceivedCallback(AudioDataReceived callback);
	void UnRegisterAudioDataReceivedCallback();
	void SetDefaultDevice(EDataFlow flow, ERole role, LPCWSTR id);

private:
	void WorkThreadImpl();
	bool TryInitialize();
	bool CaptureAudioData();
	bool InitializeOnce();
	bool InitializeImpl();
	void Quit();
	void PrivatePostThreadTask(TaskType type);
	ComPtr<IMMDevice> InitDevice(IMMDeviceEnumerator* enumerator,
		bool isDefaultDevice,
		bool isInputDevice,
		const std::string device_id);
	ComPtr<IAudioClient> InitClient(IMMDevice* device,
		bool isInputDevice,
		enum CoreAudioData::speaker_layout& speakers,
		enum CoreAudioData::audio_format& format,
		uint32_t& sampleRate);
	void ClearBuffer(IMMDevice* device);
	ComPtr<IAudioCaptureClient> InitCapture(IAudioClient* client);

private:
	bool is_input_device_ = false;
	bool is_use_default_ = false;
	bool is_use_device_time_ = false;
	std::string device_id_ = "";

	std::mutex work_mutex_;
	std::list<TaskType> work_list_;
	std::condition_variable work_cond_;
	std::thread work_thread_;

	AudioDataReceived data_received_callback_ = nullptr;

	ComPtr<IMMDeviceEnumerator> enumerator;
	ComPtr<IAudioClient> client;
	ComPtr<IAudioCaptureClient> capture;
	ComPtr<IMMNotificationClient> notify;

	CoreAudioData::speaker_layout speakers = CoreAudioData::SPEAKERS_UNKNOWN;
	CoreAudioData::audio_format format;
	uint32_t sampleRate = 0;
	WinHandle receiveSignal;
};

#endif