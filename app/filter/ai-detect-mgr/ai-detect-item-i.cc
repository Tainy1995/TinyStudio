#include "ai-detect-item-i.h"
#include "logger.h"

IAiDetectItem::IAiDetectItem()
{

}

IAiDetectItem::~IAiDetectItem()
{

}

void IAiDetectItem::UpdateDetectFrameRate(int frameRate)
{
	if (!frameRate)
		return;
	detect_frame_rate_ = frameRate;
	detect_interval_ms_ = 1000.0 / detect_frame_rate_;
}

void IAiDetectItem::InputBgraRawPixelData(uint8_t* data, size_t size, size_t width, size_t height)
{
	bool bCallDetect = false;
	std::chrono::system_clock::time_point nowTime = std::chrono::system_clock::now();
	if (!time_interval_.bStarted)
	{
		time_interval_.bStarted = true;
		time_interval_.lastDetectTime = nowTime;
		time_interval_.dateCollectTime = nowTime;
	}

	int ms = std::chrono::duration_cast<std::chrono::milliseconds>(nowTime - time_interval_.lastDetectTime).count();

	if (ms >= detect_interval_ms_ && !IsCurDetecting())
	{
		time_interval_.lastDetectTime = nowTime;
		time_interval_.detectBeginTime = nowTime;
		bCallDetect = true;
	}
	if (bCallDetect)
	{
		InputBgraRawPixelDataImpl(data, size, width, height);
		std::chrono::system_clock::time_point endTime = std::chrono::system_clock::now();
		time_interval_.totalDetectTime += std::chrono::duration_cast<std::chrono::milliseconds>(endTime - time_interval_.detectBeginTime).count();
		time_interval_.totalDetectCnt += 1;
	}
	else
	{
	}

	if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_interval_.dateCollectTime).count() >= 1000)
	{
		time_interval_.dateCollectTime = std::chrono::system_clock::now();
		time_interval_.avgDetectTime = 0;
		if (time_interval_.totalDetectCnt)
			time_interval_.avgDetectTime = (float)time_interval_.totalDetectTime / (float)time_interval_.totalDetectCnt;

		LOGGER_INFO ("cnt:%lld ,time:%lldms, avg:%fms", time_interval_.totalDetectCnt, time_interval_.totalDetectTime, time_interval_.avgDetectTime);

		time_interval_.totalDetectCnt = 0;
		time_interval_.totalDetectTime = 0;
		time_interval_.avgDetectTime = 0;
	}
}

void IAiDetectItem::GetOutputDetectResult(AiDetect::AiDetectResult& result)
{
	GetOutputDetectResultImpl(result);
}

