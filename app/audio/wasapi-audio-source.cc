#include "CoTaskMemPtr.hpp"
#include "wasapi-audio-source.h"
#include "platform.h"
#include <combaseapi.h>
#include <mmeapi.h>
#include <atomic>
#include "logger.h"

#define BUFFER_TIME_100NS (5 * 10000000)

class WASAPINotify : public IMMNotificationClient {
	std::atomic<long> refs = 0; 
	WasapiAudioSource* source_;
public:
	WASAPINotify(WasapiAudioSource* source):source_(source) {}

	STDMETHODIMP_(ULONG) AddRef()
	{
		return (ULONG)refs.fetch_add(1);
	}

	STDMETHODIMP_(ULONG) STDMETHODCALLTYPE Release()
	{
		long val = refs.fetch_add(-1);
		if (val == 0)
			delete this;
		return (ULONG)val;
	}

	STDMETHODIMP QueryInterface(REFIID riid, void** ptr)
	{
		if (riid == IID_IUnknown) {
			*ptr = (IUnknown*)this;
		}
		else if (riid == __uuidof(IMMNotificationClient)) {
			*ptr = (IMMNotificationClient*)this;
		}
		else {
			*ptr = nullptr;
			return E_NOINTERFACE;
		}

		refs.fetch_add(1);
		return S_OK;
	}

	STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role,
		LPCWSTR id)
	{
		if(source_)
			source_->SetDefaultDevice(flow, role, id);
		return S_OK;
	}

	STDMETHODIMP OnDeviceAdded(LPCWSTR) { return S_OK; }
	STDMETHODIMP OnDeviceRemoved(LPCWSTR) { return S_OK; }
	STDMETHODIMP OnDeviceStateChanged(LPCWSTR, DWORD) { return S_OK; }
	STDMETHODIMP OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY)
	{
		return S_OK;
	}
};

WasapiAudioSource::WasapiAudioSource()
{

}

WasapiAudioSource::~WasapiAudioSource()
{
	PrivatePostThreadTask(TaskType::kTaskQuit);
	if (work_thread_.joinable())
	{
		work_thread_.join();
	}

	if(enumerator)
		enumerator->UnregisterEndpointNotificationCallback(notify.Get());

	if (notify)
		notify->Release();
}

void WasapiAudioSource::RegisterAudioDataReceivedCallback(AudioDataReceived callback)
{
	data_received_callback_ = callback;
}

void WasapiAudioSource::UnRegisterAudioDataReceivedCallback()
{
	data_received_callback_ = nullptr;
}

bool WasapiAudioSource::InitWasapiDevice(bool bInputDevice, bool bUseDefault, bool bUseDeviceTime, std::string sDeviceId)
{
	is_input_device_ = bInputDevice;
	is_use_default_ = bUseDefault;
	is_use_device_time_ = bUseDeviceTime;
	device_id_ = sDeviceId;

	PrivatePostThreadTask(TaskType::kTaskInitialize);
	work_thread_ = std::thread(&WasapiAudioSource::WorkThreadImpl, this);

	return true;
}

void WasapiAudioSource::SetDefaultDevice(EDataFlow flow, ERole role, LPCWSTR id)
{

}

void WasapiAudioSource::PrivatePostThreadTask(TaskType type)
{
	std::unique_lock<std::mutex> lock(work_mutex_);
	if (type == TaskType::kTaskQuit)
		work_list_.insert(work_list_.begin(), type);
	else
		work_list_.push_back(type);
	work_cond_.notify_one();
}

void WasapiAudioSource::WorkThreadImpl()
{
	initialize_com();
	
	if (!InitializeOnce())
		goto end;

	for (;;)
	{
		TaskType task;
		{
			std::unique_lock<std::mutex> lock(work_mutex_);
			work_cond_.wait(lock, [&] {
				return !work_list_.empty();
			});
			task = work_list_.front();
			work_list_.pop_front();
		}
		switch (task)
		{
		case TaskType::kTaskInitialize:
		{
			if (!TryInitialize())
			{
				Quit();
				PrivatePostThreadTask(TaskType::kTaskInitialize);
			}
			else
			{
				PrivatePostThreadTask(TaskType::kTaskCaptureData);
			}
		}
		break;
		case TaskType::kTaskCaptureData:
		{
			const DWORD ret = WaitForMultipleObjects(1, &receiveSignal, false, 10);

			if (!CaptureAudioData())
			{
				Quit();
				PrivatePostThreadTask(TaskType::kTaskInitialize);
			}
			else
			{
				PrivatePostThreadTask(TaskType::kTaskCaptureData);
			}
		}
		break;
		case TaskType::kTaskQuit:
		{
			Quit();
			goto end;
		}
		break;
		}
	}

end:
	work_list_.clear();
	uninitialize_com();
}

bool WasapiAudioSource::TryInitialize()
{
	bool success = false;
	try
	{
		if (InitializeImpl())
		{
			success = true;
		}
	}
	catch (...)
	{
	}
	return success;
}

bool WasapiAudioSource::InitializeOnce()
{
	receiveSignal = CreateEvent(nullptr, false, false, nullptr);
	if (!receiveSignal.Valid())
		return false;
	
	notify = new WASAPINotify(this);
	if (!notify)
		return false;

	HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
		CLSCTX_ALL,
		IID_PPV_ARGS(enumerator.GetAddressOf()));
	if (FAILED(hr))
		return false;

	hr = enumerator->RegisterEndpointNotificationCallback(notify.Get());
	if (FAILED(hr))
		return false;

	return true;
}

bool WasapiAudioSource::InitializeImpl()
{
	ResetEvent(receiveSignal);

	ComPtr<IMMDevice> device = InitDevice(enumerator.Get(), is_use_default_, is_input_device_, device_id_);
	if (!device)
		return false;

	ComPtr<IAudioClient> temp_client = InitClient(device.Get(), is_input_device_, speakers, format, sampleRate);
	if (!temp_client)
		return false;

	if (!is_input_device_)
		ClearBuffer(device.Get());

	ComPtr<IAudioCaptureClient> temp_capture = InitCapture(temp_client.Get());

	client = std::move(temp_client);
	capture = std::move(temp_capture);

	return true;
}

bool WasapiAudioSource::CaptureAudioData()
{
	HRESULT res;
	LPBYTE buffer;
	UINT32 frames;
	DWORD flags;
	UINT64 pos, ts;
	UINT captureSize = 0;
	if (!capture)
		return false;
	while (true) {
		res = capture->GetNextPacketSize(&captureSize);
		if (FAILED(res)) {
			return false;
		}

		if (!captureSize)
			break;

		res = capture->GetBuffer(&buffer, &frames, &flags, &pos, &ts);
		if (FAILED(res)) {
			return false;
		}

		CoreAudioData::AudioMixerOutput data = {};
		data.data[0] = (uint8_t*)buffer;
		data.frames = (uint32_t)frames;
		data.speakers = speakers;
		data.samples_per_sec = sampleRate;
		data.format = format;
		data.timestamp = is_use_device_time_ ? ts * 100 : os_gettime_ns();

		if (!is_use_device_time_)
			data.timestamp -= util_mul_div64(frames, 1000000000ULL, sampleRate);

		if (data_received_callback_)
			data_received_callback_(&data);

		capture->ReleaseBuffer(frames);
	}

	return true;
}

void WasapiAudioSource::Quit()
{
	if (client) 
	{
		client->Stop();
		client.Reset();
	}
	if (capture)
	{
		capture.Reset();
	}
}

ComPtr<IMMDevice> WasapiAudioSource::InitDevice(IMMDeviceEnumerator* enumerator,
	bool isDefaultDevice,
	bool isInputDevice,
	const std::string device_id)
{
	ComPtr<IMMDevice> device;

	if (isDefaultDevice) {
		HRESULT res = enumerator->GetDefaultAudioEndpoint(
			isInputDevice ? eCapture : eRender,
			isInputDevice ? eCommunications : eConsole,
			device.GetAddressOf());
		if (FAILED(res))
			return device;
	}
	else {
		//wchar_t* w_id;
		//os_utf8_to_wcs_ptr(device_id.c_str(), device_id.size(), &w_id);
		//if (!w_id)
		//	throw "Failed to widen device id string";

		//const HRESULT res =
		//	enumerator->GetDevice(w_id, device.Assign());

		//bfree(w_id);

		//if (FAILED(res))
		//	return device;
	}

	return device;
}

ComPtr<IAudioClient> WasapiAudioSource::InitClient(IMMDevice* device,
	bool isInputDevice,
	enum CoreAudioData::speaker_layout& speakers,
	enum CoreAudioData::audio_format& format,
	uint32_t& sampleRate)
{
	ComPtr<IAudioClient> client;
	HRESULT res = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
		nullptr, (void**)client.GetAddressOf());
	if (FAILED(res))
		return nullptr;

	CoTaskMemPtr<WAVEFORMATEX> wfex;
	res = client->GetMixFormat(&wfex);
	if (FAILED(res))
		return nullptr;

	DWORD layout = 0;

	if (wfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		WAVEFORMATEXTENSIBLE* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>((WAVEFORMATEX*)wfex);
		layout = ext->dwChannelMask;
	}

	/* WASAPI is always float */
	speakers = CoreAudioData::convert_speaker_layout(layout, wfex->nChannels);
	format = CoreAudioData::AUDIO_FORMAT_FLOAT;
	sampleRate = wfex->nSamplesPerSec;

	DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
	if (!isInputDevice)
		flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;

	res = client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, BUFFER_TIME_100NS, 0, wfex, nullptr);
	if (FAILED(res))
		return nullptr;

	return client;
}

void WasapiAudioSource::ClearBuffer(IMMDevice* device)
{
	CoTaskMemPtr<WAVEFORMATEX> wfex;
	HRESULT res;
	LPBYTE buffer;
	UINT32 frames;
	ComPtr<IAudioClient> client;

	res = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
		(void**)client.GetAddressOf());
	if (FAILED(res))
		return;

	res = client->GetMixFormat(&wfex);
	if (FAILED(res))
		return;

	res = client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, BUFFER_TIME_100NS,
		0, wfex, nullptr);
	if (FAILED(res))
		return;

	/* Silent loopback fix. Prevents audio stream from stopping and */
	/* messing up timestamps and other weird glitches during silence */
	/* by playing a silent sample all over again. */

	res = client->GetBufferSize(&frames);
	if (FAILED(res))
		return;

	ComPtr<IAudioRenderClient> render;
	res = client->GetService(IID_PPV_ARGS(render.GetAddressOf()));
	if (FAILED(res))
		return;

	res = render->GetBuffer(frames, &buffer);
	if (FAILED(res))
		return;

	memset(buffer, 0, (size_t)frames * (size_t)wfex->nBlockAlign);

	render->ReleaseBuffer(frames, 0);
}

ComPtr<IAudioCaptureClient> WasapiAudioSource::InitCapture(IAudioClient* client)
{
	ComPtr<IAudioCaptureClient> capture;
	HRESULT res = client->GetService(IID_PPV_ARGS(capture.GetAddressOf()));
	if (FAILED(res))
		return nullptr;
	client->SetEventHandle(receiveSignal);
	res = client->Start();
	if (FAILED(res))
		return nullptr;

	return capture;
}