#pragma once

#include <Windows.h>
#include <stdint.h>
#include <memory>
#include "base-encoder-i.h"

class CoreEngine;

class IBaseOutput
{
public:
	virtual ~IBaseOutput() = default;
	void SetCoreEnv(CoreEngine* engine) { core_engine_ = engine; }
	
	virtual bool Init() = 0;
	virtual bool Update(const char* json) = 0;
	virtual void SetVideoEncoder(std::shared_ptr<IBaseEncoder> encoder) = 0;
	virtual void SetAudioEncoder(std::shared_ptr<IBaseEncoder> encoder) = 0;
	virtual void InputEncodedData(BaseEncoder::EncodedFrameData encoded_data) = 0;

protected:
	CoreEngine* core_engine_ = nullptr;
	std::shared_ptr<IBaseEncoder> video_encoder_;
	std::shared_ptr<IBaseEncoder> audio_encoder_;
};