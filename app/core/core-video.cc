#include "core-video.h"
#include "core-settings.h"
#include "core-engine.h"
#include "core-display.h"
#include "core-scene.h"
#include "core-d3d.h"
#include "core-audio.h"
#include "platform.h"
#include "logger.h"
#include "json.hpp"
#include <codecvt>

CoreVideo::CoreVideo()
{
	graphics_run_.store(false);
	video_capture_start_ts_.store(0);
	last_frame_cnt_.store(0);
}

CoreVideo::~CoreVideo()
{

}

void CoreVideo::PreEndup()
{
	graphics_run_.store(false);
	if (graphics_thread_.joinable())
		graphics_thread_.join();
}

void CoreVideo::StartupCoreVideo()
{
	video_capture_start_ts_.store(0);
	last_frame_cnt_.store(0);
	graphics_run_.store(true);
	graphics_thread_ = std::thread(&CoreVideo::GraphicsThreadImpl, this);
}

void CoreVideo::RegisterVideoDataCallback(void* ptr, CoreVideoDataCallback cb)
{
	std::unique_lock<std::mutex> lock(video_callback_mutex_);
	if (video_callback_map_.find(ptr) != video_callback_map_.end())
		return;

	video_callback_map_.insert(std::make_pair(ptr, cb));
}

void CoreVideo::UnRegisterVideoDataCallback(void* ptr)
{
	std::unique_lock<std::mutex> lock(video_callback_mutex_);
	if (video_callback_map_.find(ptr) == video_callback_map_.end())
		return;
	video_callback_map_.erase(ptr);
}

void CoreVideo::GraphicsThreadImpl()
{
	CoreSettings* settings = core_engine_->GetSettings();
	CoreDisplay* display = core_engine_->GetDisplay();
	CoreScene* scene = core_engine_->GetScene();
	CoreD3D* d3d = core_engine_->GetD3D();

	int fps = settings->GetVideoParam()->fps;
	double intervalms = 1000.0 / fps;
	uint64_t intervalns = (uint64_t)((uint64_t)1000000.0 * intervalms);
	uint64_t last_ns = os_gettime_ns();
	uint64_t start_ns = last_ns;
	intervalns = util_mul_div64(1000000000ULL, 1, fps);

	uint64_t last_metric_ns = last_ns;
	int frame_cnt = 0;
	uint64_t render_cost_ = 0;

	uint64_t count = 1;

	while (1)
	{
		if (!graphics_run_.load())
		{
			LOGGER_INFO("[Graphics] thread finished");
			return;
		}
		uint64_t timestamp = last_ns;

		UpdateFpsText();

		scene->TickSources();

		RenderOutputImpl(timestamp, count);

		display->RenderDisplay();

		frame_cnt += 1;

		render_cost_ += (os_gettime_ns() - last_ns);

		uint64_t cur_ns = os_gettime_ns();
		uint64_t past = cur_ns - last_ns;
		uint64_t sleep_ns = past > intervalns ? 0 : (intervalns - past);

		if (sleep_ns)
		{
			//UsSleep(sleep_ns / 1000.0);
			os_sleepto_ns(cur_ns + sleep_ns);
		}
		
		last_ns = os_gettime_ns();

		count = (last_ns - timestamp) / intervalns;

		if (last_ns - last_metric_ns >= 1000000000ULL)
		{
			LOGGER_INFO("[Graphics] frame num:%d render cost:%lld avg render:%f", frame_cnt, render_cost_, render_cost_ / frame_cnt / 1000000.0);
			last_frame_cnt_.store(frame_cnt);
			last_metric_ns = last_ns;
			frame_cnt = 0;
			render_cost_ = 0;
		}
	}
}

void CoreVideo::UpdateFpsText()
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> cv;
	std::wstring text_1 = L"fps:";
	text_1 += std::to_wstring(last_frame_cnt_.load());
	nlohmann::json json = {
		{"text", cv.to_bytes(text_1)}
	};

	CoreScene* scene = core_engine_->GetScene();
	scene->UpdateSourceProperty("text_fps", json.dump().c_str());
}

void CoreVideo::RenderOutputImpl(uint64_t timestamp, uint64_t count)
{

	CoreSettings* settings = core_engine_->GetSettings();
	if (!settings->GetOutputParam()->enable)
		return;

	CoreAudio* audio = core_engine_->GetAudio();
	CoreD3D* d3d = core_engine_->GetD3D();
	CoreScene* scene = core_engine_->GetScene();
	
	uint8_t* bgra_data = nullptr;
	size_t linesize = 0;

	if (!video_capture_start_ts_.load())
		video_capture_start_ts_.store(timestamp);

	if (!audio)
		return;

	if (!audio->GetCaptureStartTs())
		return;

	d3d->RenderOutputTextureBegin();
	scene->RenderSources();
	d3d->RenderOutputTextureEnd(&bgra_data, linesize);

	CoreVideoData::RawData data;
	data.bgra_data = bgra_data;
	data.linesize = linesize;
	data.timestamp = timestamp;

	{
		std::unique_lock<std::mutex> lock(video_callback_mutex_);
		for (const auto& c : video_callback_map_)
		{
			c.second(&data);
		}
	}

	free(bgra_data);
	bgra_data = nullptr;
}