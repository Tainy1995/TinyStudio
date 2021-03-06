#include <mmdeviceapi.h>
#include <mmeapi.h>
#include <combaseapi.h>
#include "wasapi-audio-monitor.h"
#include "platform.h"

extern "C"
{
#define ACTUALLY_DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
	EXTERN_C const GUID DECLSPEC_SELECTANY name = {                       \
		l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}
	ACTUALLY_DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xBCDE0395, 0xE52F, 0x467C, 0x8E,
		0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
	ACTUALLY_DEFINE_GUID(IID_IMMDeviceEnumerator, 0xA95664D2, 0x9614, 0x4F35, 0xA7,
		0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
	ACTUALLY_DEFINE_GUID(IID_IAudioClient, 0x1CB9AD4C, 0xDBFA, 0x4C32, 0xB1, 0x78,
		0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2);
	ACTUALLY_DEFINE_GUID(IID_IAudioRenderClient, 0xF294ACFC, 0x3146, 0x4483, 0xA7,
		0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2);
}

WasapiAudioMonitor::WasapiAudioMonitor()
{
	circlebuf_init(&delay_buffer);
}

WasapiAudioMonitor::~WasapiAudioMonitor()
{
	FreeContextForReconnect();

	if (com_initialized_)
		uninitialize_com();
}

bool WasapiAudioMonitor::CreateWasapiMonitor(const CoreAudioData::resample_info* input_info)
{
	if (call_once_init_)
		return true;
	call_once_init_ = true;
	com_initialized_ = initialize_com();
	memset(&input_info_, 0, sizeof(input_info_));
	memcpy(&input_info_, input_info, sizeof(CoreAudioData::resample_info));

	InitializeSRWLock(&playback_mutex_);

	if (!CreateWasapiImpl())
		return false;
	return true;
}

bool WasapiAudioMonitor::CreateWasapiImpl()
{
	if(!com_initialized_)
		com_initialized_ = initialize_com();
	bool success = false;
	IMMDeviceEnumerator* immde = nullptr;
	WAVEFORMATEX* wfex = nullptr;
	IMMDevice* device = nullptr;
	UINT32 frames;
	HRESULT hr;
	hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&immde);
	if (FAILED(hr)) {
		return false;
	}
	hr = immde->GetDefaultAudioEndpoint(eRender, eConsole, &device);
	if (FAILED(hr)) {
		goto fail;
	}
	hr = device->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&audio_client_);
	device->Release();
	if (FAILED(hr)) {
		goto fail;
	}
	hr = audio_client_->GetMixFormat(&wfex);
	if (FAILED(hr)) {
		goto fail;
	}
	hr = audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED, 0,
		10000000, 0, wfex, NULL);
	if (FAILED(hr)) {
		goto fail;
	}

	WAVEFORMATEXTENSIBLE* ext = (WAVEFORMATEXTENSIBLE*)wfex;
	struct CoreAudioData::resample_info from;
	struct CoreAudioData::resample_info to;

	from.samples_per_sec = input_info_.samples_per_sec;
	from.speakers = input_info_.speakers;
	from.format = input_info_.format;

	to.samples_per_sec = (uint32_t)wfex->nSamplesPerSec;
	to.speakers = CoreAudioData::convert_speaker_layout(ext->dwChannelMask, wfex->nChannels);
	to.format = CoreAudioData::AUDIO_FORMAT_FLOAT;

	sample_rate_ = (uint32_t)wfex->nSamplesPerSec;
	channels_ = wfex->nChannels;
	if (!audio_resample_)
		audio_resample_ = new AudioResampler();
	if (!audio_resample_->CreateAudioResampler(&from, &to))
		goto fail;

	hr = audio_client_->GetBufferSize(&frames);
	if (FAILED(hr)) {
		goto fail;
	}

	hr = audio_client_->GetService(IID_IAudioRenderClient,(void**)&audio_render_);
	if (FAILED(hr)) {
		goto fail;
	}

	hr = audio_client_->Start();
	if (FAILED(hr)) {
		goto fail;
	}
	success = true;
	return true;

fail:
	if (immde)
	{
		immde->Release();
	}
	if (wfex)
		CoTaskMemFree(wfex);
	if (audio_resample_)
	{
		delete audio_resample_;
		audio_resample_ = nullptr;
	}
	return false;
}

void WasapiAudioMonitor::FreeContextForReconnect()
{
	if (audio_client_)
	{
		audio_client_->Stop();
	}
	if (audio_render_)
	{
		audio_render_->Release();
		audio_render_ = nullptr;
	}
	if (audio_client_)
	{
		audio_client_->Stop();
		audio_client_->Release();
		audio_client_ = nullptr;
	}
	if (audio_resample_)
	{
		delete audio_resample_;
		audio_resample_ = nullptr;
	}
	circlebuf_free(&delay_buffer);
	buf_.clear();
	if (com_initialized_)
	{
		uninitialize_com();
		com_initialized_ = false;
	}
}

void WasapiAudioMonitor::InputAudioData(const uint8_t* const input[], uint32_t in_frames, uint64_t timestamp)
{
	uint8_t* resample_data[MAX_AV_PLANES];
	uint32_t resample_frames;
	uint64_t ts_offset;
	bool success;
	BYTE* output;
	if (!TryAcquireSRWLockExclusive(&playback_mutex_)) {
		return;
	}
	if (!audio_client_ && !CreateWasapiImpl()) {
		goto free_for_reconnect;
	}
	success = audio_resample_->Resampler(resample_data, &resample_frames, &ts_offset,
		(const uint8_t* const*)input, (uint32_t)in_frames);
	if (!success) {
		goto unlock;
	}
	UINT32 pad = 0;
	HRESULT hr = audio_client_->GetCurrentPadding(&pad);
	if (FAILED(hr)) {
		goto free_for_reconnect;
	}
	uint64_t ts = timestamp - ts_offset;

	if (!ProcessAudioDelay((float**)(&resample_data[0]), &resample_frames, ts, pad)) {
		goto unlock;
	}

	IAudioRenderClient* const render = audio_render_;
	hr = render->GetBuffer(resample_frames, &output);
	if (FAILED(hr)) {
		goto free_for_reconnect;
	}
	memcpy(output, resample_data[0], resample_frames * channels_ * sizeof(float));

	hr = render->ReleaseBuffer(resample_frames, 0);
	if (FAILED(hr)) {
		goto free_for_reconnect;
	}
	goto unlock;

free_for_reconnect:
	FreeContextForReconnect();
unlock:
	ReleaseSRWLockExclusive(&playback_mutex_);
}

bool WasapiAudioMonitor::ProcessAudioDelay(float** data, uint32_t* frames, uint64_t ts, uint32_t pad)
{
#if 1
	uint64_t last_frame_ts = last_frame_ts_;
	uint64_t cur_time = os_gettime_ns();
	uint64_t front_ts;
	uint64_t cur_ts;
	int64_t diff;
	uint32_t blocksize = channels_ * sizeof(float);

	if (cur_time - last_recv_time_ > 1000000000)
		circlebuf_free(&delay_buffer);
	last_recv_time_ = cur_time;

	circlebuf_push_back(&delay_buffer, &ts, sizeof(ts));
	circlebuf_push_back(&delay_buffer, frames, sizeof(*frames));
	circlebuf_push_back(&delay_buffer, *data, *frames * blocksize);

	if (!prev_video_ts_) {
		prev_video_ts_ = last_frame_ts;

	}
	else if (prev_video_ts_ == last_frame_ts) {
		time_since_prev_ += util_mul_div64(
			*frames, 1000000000ULL, sample_rate_);
	}
	else {
		time_since_prev_ = 0;
	}

	while (delay_buffer.size != 0) {
		size_t size;
		bool bad_diff;

		circlebuf_peek_front(&delay_buffer, &cur_ts, sizeof(ts));
		front_ts = cur_ts - util_mul_div64(pad, 1000000000ULL, sample_rate_);
		diff = (int64_t)front_ts - (int64_t)last_frame_ts;
		bad_diff = !last_frame_ts || llabs(diff) > 5000000000 || time_since_prev_ > 100000000ULL;

		if (!bad_diff && diff > 75000000) {
			return false;
		}

		circlebuf_pop_front(&delay_buffer, NULL, sizeof(ts));
		circlebuf_pop_front(&delay_buffer, frames, sizeof(*frames));

		size = *frames * blocksize;
		buf_.resize(size);
		circlebuf_pop_front(&delay_buffer, &buf_[0], size);

		if (!bad_diff && diff < -75000000 && delay_buffer.size > 0) {
			continue;
		}

		*data = &buf_[0];
		return true;
	}
#endif
	return false;
}