#pragma once

#include <stdint.h>

namespace BaseEncoder
{
	struct EncodedFrameData
	{
		uint8_t* data = nullptr;
		size_t size = 0;
		int64_t pts = 0;
		int64_t dts = 0;
		bool keyframe = false;
		bool bAudio = false;
		int64_t ts_ns_ = 0;
	};
}