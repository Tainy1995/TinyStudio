#ifndef AI_DETECT_ITEM_I_H
#define AI_DETECT_ITEM_I_H

#include "ai-detect-define.h"
#include <vector>
#include <string>
#include <chrono>

class IAiDetectItem
{

	struct DetectTimeInterval
	{
		bool bStarted = false;
		std::chrono::system_clock::time_point lastDetectTime;
		std::chrono::system_clock::time_point detectBeginTime;
		std::chrono::system_clock::time_point dateCollectTime;
		int64_t totalDetectTime = 0;
		int64_t totalDetectCnt = 0;
		float avgDetectTime = 0.0f;
	};

public:
	IAiDetectItem();
	virtual ~IAiDetectItem();
	
	void UpdateDetectFrameRate(int frameRate);
	void InputBgraRawPixelData(uint8_t* data, size_t size, size_t width, size_t height);
	void GetOutputDetectResult(AiDetect::AiDetectResult& result);
	virtual bool IsCurDetecting() = 0;

protected:
	virtual void InputBgraRawPixelDataImpl(uint8_t* data, size_t size, size_t width, size_t height) = 0;
	virtual void GetOutputDetectResultImpl(AiDetect::AiDetectResult& result) = 0;

private:
	int detect_frame_rate_ = 15;
	int detect_interval_ms_ = 66;

	DetectTimeInterval time_interval_;
};

#endif