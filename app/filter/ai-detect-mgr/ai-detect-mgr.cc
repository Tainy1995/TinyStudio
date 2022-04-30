#include "ai-detect-mgr.h"
#include "item-face-cnn.h"

AiDetectMgr::AiDetectMgr()
{

}

AiDetectMgr::~AiDetectMgr()
{
	for (auto& c = render_thread_detect_vec_.begin(); c != render_thread_detect_vec_.end();)
	{
		IAiDetectItem* item = *c;
		c = render_thread_detect_vec_.erase(c);
		delete item;
		item = nullptr;
	}
	for (auto& c = independ_thread_detect_vec_.begin(); c != independ_thread_detect_vec_.end();)
	{
		ThreadDetectItem* item = *c;
		c = independ_thread_detect_vec_.erase(c);
		{
			std::unique_lock<std::mutex> lock(item->mutex);
			item->bFinish = true;
		}
		item->cond.notify_one();
		item->thread->join();
		delete item->thread;
		item->thread = nullptr;
		delete item->detectItem;
		item->detectItem = nullptr;
		delete item;
		item = nullptr;
	}
}

void AiDetectMgr::InitAiDetectMgr(const std::vector< AiDetect::AiDetectInput >& inputVec)
{
	for (const auto& c : inputVec)
	{
		IAiDetectItem* item = nullptr;
		switch (c.detectType)
		{
		case AiDetect::AiDetectType::KAiDetectFace:
		{
			item = new ItemFaceCnn();
		}
		break;
		}

		if (!item)
			continue;

		item->UpdateDetectFrameRate(c.detectFrameRate);

		switch (c.threadType)
		{
		case AiDetect::AiDetectThreadType::KAiDetectRenderThread:
		{
			render_thread_detect_vec_.push_back(item);
		}
		break;
		case AiDetect::AiDetectThreadType::KAiDetectIndependThread:
		{
			ThreadDetectItem* threadItem = new ThreadDetectItem();
			threadItem->detectItem = item;
			threadItem->thread = new std::thread(&AiDetectMgr::ThreadDetectImpl, this, threadItem);
			independ_thread_detect_vec_.push_back(threadItem);
		}
		break;
		}
	}
}

void AiDetectMgr::ThreadDetectImpl(ThreadDetectItem* item)
{
	ThreadDetectItem* instance = item;
	for (;;) {
		ThreadDetectData data;
		std::unique_lock<std::mutex> lock(instance->mutex);

		instance->cond.wait(lock, [&] {
			return !instance->data.empty() || instance->bFinish;
			});
		if (instance->bFinish && instance->data.empty())
			break;

		data = instance->data.front();
		instance->data.pop_front();

		instance->detectItem->InputBgraRawPixelData(data.data, data.size, data.width, data.height);

		free(data.data);
		data.data = nullptr;
	}
}

void AiDetectMgr::InputBgraRawPixelData(uint8_t* data, size_t size, size_t width, size_t height)
{
	for (const auto& c : render_thread_detect_vec_)
	{
		c->InputBgraRawPixelData(data, size, width, height);
	}

	for (const auto& c : independ_thread_detect_vec_)
	{
		if (c->detectItem->IsCurDetecting())
			continue;

		ThreadDetectData threaddata;
		threaddata.data = (uint8_t*)malloc(size);
		memset(threaddata.data, 0, size);
		memcpy_s(threaddata.data, size, data, size);
		threaddata.size = size;
		threaddata.width = width;
		threaddata.height = height;

		std::unique_lock<std::mutex> lock(c->mutex);
		c->data.push_back(threaddata);
		c->cond.notify_one();
	}
}

void AiDetectMgr::GetOutputDetectResult(std::vector< AiDetect::AiDetectResult >& resultVec)
{
	resultVec.clear();
	for (const auto& c : render_thread_detect_vec_)
	{
		AiDetect::AiDetectResult result;
		c->GetOutputDetectResult(result);
		resultVec.push_back(result);
	}
	for (const auto& c : independ_thread_detect_vec_)
	{
		AiDetect::AiDetectResult result;
		c->detectItem->GetOutputDetectResult(result);
		resultVec.push_back(result);
	}
}