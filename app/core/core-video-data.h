#pragma once
#include <stdint.h>

namespace CoreVideoData
{
	struct RawData
	{
		uint8_t* bgra_data;
		size_t linesize;
		uint64_t timestamp;
	};
}