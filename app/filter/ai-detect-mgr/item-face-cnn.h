#ifndef ITEM_FACE_CNN_H
#define ITEM_FACE_CNN_H

#include "ai-detect-item-i.h"
#include <vector>
#include <string>
#include <mutex>

class ItemFaceCnn : public IAiDetectItem
{
public:
	ItemFaceCnn();
	virtual ~ItemFaceCnn();

	virtual void InputBgraRawPixelDataImpl(uint8_t* data, size_t size, size_t width, size_t height);
	virtual void GetOutputDetectResultImpl(AiDetect::AiDetectResult& result);
	virtual bool IsCurDetecting();

private:
	std::string result_buffer_;
	AiDetect::AiDetectResult face_result_;
	std::mutex mutex_;
	std::atomic<bool> is_detecting_;
};

#endif