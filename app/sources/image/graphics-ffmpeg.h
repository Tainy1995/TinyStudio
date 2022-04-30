#pragma once
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

struct ffmpeg_image {
	const char *file;
	AVFormatContext *fmt_ctx;
	AVCodecContext *decoder_ctx;
	AVCodec *decoder;
	AVStream *stream;
	int stream_idx;

	int cx, cy;
	enum AVPixelFormat format;
};

enum gs_color_format {
	GS_UNKNOWN,
	GS_A8,
	GS_R8,
	GS_RGBA,
	GS_BGRX,
	GS_BGRA,
	GS_R10G10B10A2,
	GS_RGBA16,
	GS_R16,
	GS_RGBA16F,
	GS_RGBA32F,
	GS_RG16F,
	GS_RG32F,
	GS_R16F,
	GS_R32F,
	GS_DXT1,
	GS_DXT3,
	GS_DXT5,
	GS_R8G8,
};


void gs_init_image_deps(void);

void gs_free_image_deps(void);

uint8_t* gs_create_texture_file_data(const char* file,
	enum gs_color_format* format,
	uint32_t* cx_out, uint32_t* cy_out, int& size);
