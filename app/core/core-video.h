#ifndef CORE_VIDEO_H
#define CORE_VIDEO_H

#include "core-component-i.h"
#include "core-video-data.h"
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <map>

using CoreVideoDataCallback = std::function<void(const CoreVideoData::RawData* data)>;

class CoreVideo : public ICoreComponent
{
public:
	CoreVideo();
	virtual ~CoreVideo();

	void PreEndup();
	void StartupCoreVideo();
	void RegisterVideoDataCallback(void*ptr, CoreVideoDataCallback cb);
	void UnRegisterVideoDataCallback(void* ptr);
	uint64_t GetCaptureStartTs() { return video_capture_start_ts_.load(); }
	uint64_t GetLastFrameCnt() { return last_frame_cnt_.load(); }

private:
	void GraphicsThreadImpl();
	void RenderOutputImpl(uint64_t timestamp, uint64_t count);
	void UpdateFpsText();

private:
	std::thread graphics_thread_;
	std::atomic<bool> graphics_run_;

	std::mutex video_callback_mutex_;
	std::map< void*, CoreVideoDataCallback> video_callback_map_;

	std::atomic<uint64_t> video_capture_start_ts_;
	std::atomic<uint64_t> last_frame_cnt_;


};

#endif