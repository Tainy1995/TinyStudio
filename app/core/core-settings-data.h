#pragma once
#include <stdint.h>
#include <Windows.h>
#include "core-audio-data.h"

namespace CoreSettingsData
{
	struct Video
	{
		size_t width;
		size_t height;
		size_t fps;
		uint32_t bitrate;
	};

	struct VideoBuilder
	{
		struct Video video;
		VideoBuilder& width(size_t _width)
		{
			video.width = _width;
			return *this;
		}
		VideoBuilder& height(size_t _height)
		{
			video.height = _height;
			return *this;
		}
		VideoBuilder& fps(size_t _fps)
		{
			video.fps = _fps;
			return *this;
		}
		VideoBuilder& bitrate(uint32_t _bitrate)
		{
			video.bitrate = _bitrate;
			return *this;
		}
	};


	struct Audio
	{
		enum CoreAudioData::audio_format audio_format;
		enum CoreAudioData::speaker_layout speakers;
		uint32_t samples_per_sec;
		uint32_t bitrate;
	};

	struct AudioBuilder
	{
		struct Audio audio;
		AudioBuilder& audio_format(enum CoreAudioData::audio_format _format)
		{
			audio.audio_format = _format;
			return *this;
		}
		AudioBuilder& speakers(enum CoreAudioData::speaker_layout _speakers)
		{
			audio.speakers = _speakers;
			return *this;
		}
		AudioBuilder& samples_per_sec(uint32_t _samples_per_sec)
		{
			audio.samples_per_sec = _samples_per_sec;
			return *this;
		}
		AudioBuilder& bitrate(uint32_t _bitrate)
		{
			audio.bitrate = _bitrate;
			return *this;
		}
	};

	struct Output
	{
		bool enable;
		const char* path;
		Video video;
		Audio audio;
	};

	struct OutputBuilder
	{
		struct Output output;
		OutputBuilder& enable(bool _enable)
		{
			output.enable = _enable;
			return *this;
		}
		OutputBuilder& path(const char* _path)
		{
			output.path = _path;
			return *this;
		}
		OutputBuilder& video(const Video* _video)
		{
			memcpy(&output.video, _video, sizeof(Video));
			return *this;
		}
		OutputBuilder& audio(const Audio* _audio)
		{
			memcpy(&output.audio, _audio, sizeof(Audio));
			return *this;
		}
	};

}