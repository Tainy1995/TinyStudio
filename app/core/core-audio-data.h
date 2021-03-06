#pragma once
#include <stdint.h>
#include <Windows.h>
#include <ks.h>
#include <ksmedia.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include "libswresample/swresample.h"
}

#define MAX_AV_PLANES 8
#define MAX_AUDIO_MIXES 6
#define MAX_AUDIO_CHANNELS 8
#define AUDIO_OUTPUT_FRAMES 1024
#ifndef KSAUDIO_SPEAKER_4POINT1
#define KSAUDIO_SPEAKER_4POINT1 \
	(KSAUDIO_SPEAKER_SURROUND | SPEAKER_LOW_FREQUENCY)
#endif
namespace CoreAudioData
{
	enum audio_format {
		AUDIO_FORMAT_UNKNOWN,

		AUDIO_FORMAT_U8BIT,
		AUDIO_FORMAT_16BIT,
		AUDIO_FORMAT_32BIT,
		AUDIO_FORMAT_FLOAT,

		AUDIO_FORMAT_U8BIT_PLANAR,
		AUDIO_FORMAT_16BIT_PLANAR,
		AUDIO_FORMAT_32BIT_PLANAR,
		AUDIO_FORMAT_FLOAT_PLANAR,
	};

	enum speaker_layout {
		SPEAKERS_UNKNOWN,     /**< Unknown setting, fallback is stereo. */
		SPEAKERS_MONO,        /**< Channels: MONO */
		SPEAKERS_STEREO,      /**< Channels: FL, FR */
		SPEAKERS_2POINT1,     /**< Channels: FL, FR, LFE */
		SPEAKERS_4POINT0,     /**< Channels: FL, FR, FC, RC */
		SPEAKERS_4POINT1,     /**< Channels: FL, FR, FC, LFE, RC */
		SPEAKERS_5POINT1,     /**< Channels: FL, FR, FC, LFE, RL, RR */
		SPEAKERS_7POINT1 = 8, /**< Channels: FL, FR, FC, LFE, RL, RR, SL, SR */
	};

	struct resample_info {
		uint32_t samples_per_sec;
		enum audio_format format;
		enum speaker_layout speakers;
	};

	struct AudioMixerOutput
	{
		uint8_t* data[MAX_AV_PLANES];
		uint32_t frames;
		enum speaker_layout speakers;
		enum audio_format format;
		uint32_t samples_per_sec;
		uint64_t timestamp;
	};

	static inline enum audio_format convert_sample_format(int f)
	{
		switch (f) {
		case AV_SAMPLE_FMT_U8:
			return AUDIO_FORMAT_U8BIT;
		case AV_SAMPLE_FMT_S16:
			return AUDIO_FORMAT_16BIT;
		case AV_SAMPLE_FMT_S32:
			return AUDIO_FORMAT_32BIT;
		case AV_SAMPLE_FMT_FLT:
			return AUDIO_FORMAT_FLOAT;
		case AV_SAMPLE_FMT_U8P:
			return AUDIO_FORMAT_U8BIT_PLANAR;
		case AV_SAMPLE_FMT_S16P:
			return AUDIO_FORMAT_16BIT_PLANAR;
		case AV_SAMPLE_FMT_S32P:
			return AUDIO_FORMAT_32BIT_PLANAR;
		case AV_SAMPLE_FMT_FLTP:
			return AUDIO_FORMAT_FLOAT_PLANAR;
		default:;
		}

		return AUDIO_FORMAT_UNKNOWN;
	}


	static inline enum AVSampleFormat convert_audio_format(enum audio_format format)
	{
		switch (format) {
		case AUDIO_FORMAT_UNKNOWN:
			return AV_SAMPLE_FMT_S16;
		case AUDIO_FORMAT_U8BIT:
			return AV_SAMPLE_FMT_U8;
		case AUDIO_FORMAT_16BIT:
			return AV_SAMPLE_FMT_S16;
		case AUDIO_FORMAT_32BIT:
			return AV_SAMPLE_FMT_S32;
		case AUDIO_FORMAT_FLOAT:
			return AV_SAMPLE_FMT_FLT;
		case AUDIO_FORMAT_U8BIT_PLANAR:
			return AV_SAMPLE_FMT_U8P;
		case AUDIO_FORMAT_16BIT_PLANAR:
			return AV_SAMPLE_FMT_S16P;
		case AUDIO_FORMAT_32BIT_PLANAR:
			return AV_SAMPLE_FMT_S32P;
		case AUDIO_FORMAT_FLOAT_PLANAR:
			return AV_SAMPLE_FMT_FLTP;
		}

		/* shouldn't get here */
		return AV_SAMPLE_FMT_S16;
	}

	static inline enum speaker_layout convert_speaker_layout(uint8_t channels)
	{
		switch (channels) {
		case 0:
			return SPEAKERS_UNKNOWN;
		case 1:
			return SPEAKERS_MONO;
		case 2:
			return SPEAKERS_STEREO;
		case 3:
			return SPEAKERS_2POINT1;
		case 4:
			return SPEAKERS_4POINT0;
		case 5:
			return SPEAKERS_4POINT1;
		case 6:
			return SPEAKERS_5POINT1;
		case 8:
			return SPEAKERS_7POINT1;
		default:
			return SPEAKERS_UNKNOWN;
		}

	}
	static enum speaker_layout convert_speaker_layout(DWORD layout, WORD channels)
	{
		switch (layout) {
		case KSAUDIO_SPEAKER_2POINT1:
			return SPEAKERS_2POINT1;
		case KSAUDIO_SPEAKER_SURROUND:
			return SPEAKERS_4POINT0;
		case KSAUDIO_SPEAKER_4POINT1:
			return SPEAKERS_4POINT1;
		case KSAUDIO_SPEAKER_5POINT1:
			return SPEAKERS_5POINT1;
		case KSAUDIO_SPEAKER_7POINT1:
			return SPEAKERS_7POINT1;
		}

		return (enum speaker_layout)channels;
	}

	static inline uint64_t convert_speaker_layout(enum speaker_layout layout)
	{
		switch (layout) {
		case SPEAKERS_UNKNOWN:
			return 0;
		case SPEAKERS_MONO:
			return AV_CH_LAYOUT_MONO;
		case SPEAKERS_STEREO:
			return AV_CH_LAYOUT_STEREO;
		case SPEAKERS_2POINT1:
			return AV_CH_LAYOUT_SURROUND;
		case SPEAKERS_4POINT0:
			return AV_CH_LAYOUT_4POINT0;
		case SPEAKERS_4POINT1:
			return AV_CH_LAYOUT_4POINT1;
		case SPEAKERS_5POINT1:
			return AV_CH_LAYOUT_5POINT1_BACK;
		case SPEAKERS_7POINT1:
			return AV_CH_LAYOUT_7POINT1;
		}

		/* shouldn't get here */
		return 0;
	}

	static inline size_t get_audio_bytes_per_channel(enum audio_format format)
	{
		switch (format) {
		case AUDIO_FORMAT_U8BIT:
		case AUDIO_FORMAT_U8BIT_PLANAR:
			return 1;

		case AUDIO_FORMAT_16BIT:
		case AUDIO_FORMAT_16BIT_PLANAR:
			return 2;

		case AUDIO_FORMAT_FLOAT:
		case AUDIO_FORMAT_FLOAT_PLANAR:
		case AUDIO_FORMAT_32BIT:
		case AUDIO_FORMAT_32BIT_PLANAR:
			return 4;

		case AUDIO_FORMAT_UNKNOWN:
			return 0;
		}

		return 0;
	}

	static inline uint32_t get_audio_channels(enum speaker_layout speakers)
	{
		switch (speakers) {
		case SPEAKERS_MONO:
			return 1;
		case SPEAKERS_STEREO:
			return 2;
		case SPEAKERS_2POINT1:
			return 3;
		case SPEAKERS_4POINT0:
			return 4;
		case SPEAKERS_4POINT1:
			return 5;
		case SPEAKERS_5POINT1:
			return 6;
		case SPEAKERS_7POINT1:
			return 8;
		case SPEAKERS_UNKNOWN:
			return 0;
		}

		return 0;
	}

	static inline bool is_audio_planar(enum audio_format format)
	{
		switch (format) {
		case AUDIO_FORMAT_U8BIT:
		case AUDIO_FORMAT_16BIT:
		case AUDIO_FORMAT_32BIT:
		case AUDIO_FORMAT_FLOAT:
			return false;

		case AUDIO_FORMAT_U8BIT_PLANAR:
		case AUDIO_FORMAT_FLOAT_PLANAR:
		case AUDIO_FORMAT_16BIT_PLANAR:
		case AUDIO_FORMAT_32BIT_PLANAR:
			return true;

		case AUDIO_FORMAT_UNKNOWN:
			return false;
		}

		return false;
	}

	static inline size_t get_audio_size(enum audio_format format,
		enum speaker_layout speakers,
		uint32_t frames)
	{
		bool planar = is_audio_planar(format);

		return (planar ? 1 : get_audio_channels(speakers)) *
			get_audio_bytes_per_channel(format) * frames;
	}

	static inline size_t get_audio_planes(enum audio_format format,
		enum speaker_layout speakers)
	{
		return (is_audio_planar(format) ? get_audio_channels(speakers) : 1);
	}
}