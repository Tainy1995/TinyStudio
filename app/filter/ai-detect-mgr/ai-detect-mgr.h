#ifndef AI_DETECT_MGR_H
#define AI_DETECT_MGR_H

#include "ai-detect-define.h"
#include <vector>
#include <string>
#include <thread>
#include <condition_variable>
#include <list>
#include "ai-detect-item-i.h"

class AiDetectMgr
{
	struct ThreadDetectData
	{
		uint8_t* data = nullptr;
		size_t size;
		size_t width;
		size_t height;
	};

	struct ThreadDetectItem
	{
		std::mutex mutex;
		std::condition_variable cond;
		bool bFinish = false;
		std::thread* thread = nullptr;
		IAiDetectItem* detectItem = nullptr;
		std::list<struct ThreadDetectData> data;
	};

public:
	AiDetectMgr();
	~AiDetectMgr();

	void InitAiDetectMgr(const std::vector< AiDetect::AiDetectInput >& inputVec);
	void InputBgraRawPixelData(uint8_t* data, size_t size, size_t width, size_t height);
	void GetOutputDetectResult(std::vector< AiDetect::AiDetectResult >& resultVec);

private:
	void ThreadDetectImpl(ThreadDetectItem* item);

private:
	std::vector< IAiDetectItem* > render_thread_detect_vec_;
	std::vector< ThreadDetectItem* > independ_thread_detect_vec_;
};


#endif