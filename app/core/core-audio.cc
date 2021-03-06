#include "core-audio.h"
#include "core-settings.h"
#include "core-engine.h"
#include "core-display.h"
#include "core-scene.h"
#include "core-video.h"
#include "platform.h"
#include "logger.h"

CoreAudio::CoreAudio()
{
	audio_run_.store(false);
	audio_capture_start_ts_.store(0);
	for (size_t i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		circlebuf_init(&desktop_audio_buf_[i]);
	}
}

CoreAudio::~CoreAudio()
{
	if (desktop_audio_)
	{
		delete desktop_audio_;
		desktop_audio_ = nullptr;
	}

	if (audio_resampler_)
	{
		delete audio_resampler_;
		audio_resampler_ = nullptr;
	}

	for (size_t i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		if (desktop_audio_buf_[i].size)
			circlebuf_pop_front(&desktop_audio_buf_[i], NULL, desktop_audio_buf_[i].size);
		circlebuf_free(&desktop_audio_buf_[i]);
	}
}

void CoreAudio::PreEndup()
{
	if (desktop_audio_)
	{
		desktop_audio_->UnRegisterAudioDataReceivedCallback();
	}
	audio_run_.store(false);
	if (audio_thread_.joinable())
		audio_thread_.join();
}

void CoreAudio::StartupCoreAudio()
{
	audio_run_.store(true);
	audio_capture_start_ts_.store(0);
	audio_thread_ = std::thread(&CoreAudio::AudioThreadImpl, this);
}

void CoreAudio::AudioThreadImpl()
{
	CoreSettings* settings = core_engine_->GetSettings();
	if (settings->GetOutputParam()->enable)
	{
		desktop_audio_ = new WasapiAudioSource();
		desktop_audio_->InitWasapiDevice(false, true, false);
		desktop_audio_->RegisterAudioDataReceivedCallback(std::bind(&CoreAudio::DesktopAudioDataReceived, this, std::placeholders::_1));
	}

	uint32_t rate = settings->GetAudioParam()->samples_per_sec;
	double intervalms = (uint32_t)(audio_frames_to_ns(rate, AUDIO_OUTPUT_FRAMES) / 1000000);
	uint64_t intervalns = (uint64_t)(1000000.0 * intervalms);
	uint64_t last_ns = os_gettime_ns();
	uint64_t start_ns = last_ns;
	uint64_t audio_time = last_ns;
	uint64_t samples_ = 0;

	uint64_t last_metric_ns = last_ns;
	int frame_cnt = 0;
	uint64_t render_cost_ = 0;
	while (1)
	{
		if (!audio_run_.load())
		{
			LOGGER_INFO("[Audio] thread finished");
			return;
		}
		uint64_t timestamp = last_ns;

		while (audio_time <= timestamp)
		{
			samples_ += AUDIO_OUTPUT_FRAMES;
			audio_time = start_ns + audio_frames_to_ns(rate, samples_);
			OutputCoreAudioData(audio_time);
			frame_cnt += 1;
		}

		uint64_t cur_ns = os_gettime_ns();
		uint64_t sleep_ns = (cur_ns - last_ns) > intervalns ? 0 : intervalns - (cur_ns - last_ns);

		if (sleep_ns)
		{
			os_sleepto_ns(cur_ns + sleep_ns);
		}

		last_ns = os_gettime_ns();

		if (last_ns - last_metric_ns >= 1000000000ULL)
		{
			LOGGER_INFO("[Audio] frame num:%d render cost:%lld avg render:%f", frame_cnt, render_cost_, render_cost_ / frame_cnt / 1000000.0);
			last_metric_ns = last_ns;
			frame_cnt = 0;
			render_cost_ = 0;
		}
	}
}

void CoreAudio::DesktopAudioDataReceived(const CoreAudioData::AudioMixerOutput* output)
{
	if (!audio_capture_start_ts_.load())
	{
		audio_capture_start_ts_.store(os_gettime_ns());
	}
	CoreSettings* settings = core_engine_->GetSettings();
	CoreVideo* video = core_engine_->GetVideo();
	if (!video)
		return;
	if (!video->GetCaptureStartTs())
		return;
	bool success = false;
	uint64_t ts_offset = 0;
	CoreAudioData::AudioMixerOutput outputdata;
	if (!audio_resampler_)
	{
		struct CoreAudioData::resample_info from;
		struct CoreAudioData::resample_info to;
		from.samples_per_sec = output->samples_per_sec;
		from.speakers = output->speakers;
		from.format = output->format;

		to.samples_per_sec = settings->GetOutputParam()->audio.samples_per_sec;
		to.speakers = settings->GetOutputParam()->audio.speakers;
		to.format = settings->GetOutputParam()->audio.audio_format;

		audio_resampler_ = new AudioResampler();
		audio_resampler_->CreateAudioResampler(&from, &to);
	}
	success = audio_resampler_->Resampler(outputdata.data, &outputdata.frames, &ts_offset, (const uint8_t* const*)output->data, (uint32_t)output->frames);
	if (success)
	{
		outputdata.speakers = settings->GetOutputParam()->audio.speakers;
		outputdata.format = settings->GetOutputParam()->audio.audio_format;
		outputdata.samples_per_sec = settings->GetOutputParam()->audio.samples_per_sec;
		outputdata.timestamp = output->timestamp - ts_offset;

		std::unique_lock<std::mutex> lock(desktop_audio_mutex_);
		int channels = CoreAudioData::get_audio_channels(outputdata.speakers);
		for (int i = 0; i < channels; i++)
		{
			circlebuf_push_back(&desktop_audio_buf_[i], outputdata.data[i], outputdata.frames * sizeof(float));
		}
	}
}

void CoreAudio::RegisterCoreAudioDataCallback(void* ptr, CoreAudioDataCallback cb)
{
	std::unique_lock<std::mutex> lock(audio_callback_mutex_);
	if (audio_callback_map_.find(ptr) != audio_callback_map_.end())
		return;
	audio_callback_map_.insert(std::make_pair(ptr, cb));
}

void CoreAudio::UnRegisterCoreAudioDataCallback(void* ptr)
{
	std::unique_lock<std::mutex> lock(audio_callback_mutex_);
	if (audio_callback_map_.find(ptr) == audio_callback_map_.end())
		return;
	audio_callback_map_.erase(ptr);
}

void CoreAudio::OutputCoreAudioData(uint64_t timestamp)
{
	CoreSettings* settings = core_engine_->GetSettings();
	if (!settings->GetOutputParam()->enable)
		return;

	std::unique_lock<std::mutex> lock(desktop_audio_mutex_);
	int channels = CoreAudioData::get_audio_channels(settings->GetOutputParam()->audio.speakers);
	if (desktop_audio_buf_[0].size >= 1024 * sizeof(float))
	{
		uint8_t* output_data[MAX_AV_PLANES] = { 0 };
		size_t linsize[MAX_AV_PLANES] = { 0 };
		for (int i = 0; i < channels; i++)
		{
			output_data[i] = (uint8_t*)malloc(1024 * sizeof(float));
			circlebuf_pop_front(&desktop_audio_buf_[i], output_data[i], 1024 * sizeof(float));
		}
		CoreAudioData::AudioMixerOutput outputdata;
		outputdata.frames = AUDIO_OUTPUT_FRAMES;
		for (int i = 0; i < MAX_AV_PLANES; i++)
		{
			outputdata.data[i] = output_data[i];
		}
		outputdata.speakers = settings->GetOutputParam()->audio.speakers;
		outputdata.format = settings->GetOutputParam()->audio.audio_format;
		outputdata.samples_per_sec = settings->GetOutputParam()->audio.samples_per_sec;
		outputdata.timestamp = timestamp;

		for (const auto& c : audio_callback_map_)
		{
			c.second(&outputdata);
		}

		for (int i = 0; i < channels; i++)
		{
			if (output_data[i])
			{
				free(output_data[i]);
			}
		}
	}
}