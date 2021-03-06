#pragma once

#include <Windows.h>
#include <stdint.h>
#include "base-encoder-data.h"

class CoreEngine;

class IBaseEncoder
{
public:
	virtual ~IBaseEncoder() = default;
	void SetCoreEnv(CoreEngine* engine) { core_engine_ = engine; }
	
	virtual bool Init() = 0;
	virtual bool Update(const char* json) = 0;
	virtual void InputRawData(uint8_t** data, size_t* lineSize, int plane, uint64_t pts, bool* receiveFrame) = 0;
	virtual void GetEncodedData(BaseEncoder::EncodedFrameData* data) = 0;
	virtual void GetEncoderExtraData(uint8_t** data, size_t* size) = 0;

protected:
	CoreEngine* core_engine_ = nullptr;
};