#ifndef X264_ENCODER_H
#define X264_ENCODER_H

#include <vector>
#include <string>
#include "base-encoder-i.h"
#include "x264.h"

class X264Encoder : public IBaseEncoder
{
	struct X264Param
	{
		int bitrate = 2500;
		bool use_bufsize = false;
		int buffer_size = 2500;
		int keyint_sec = 0;
		int crf = 23;
		std::string rate_control = "CBR";
		std::string preset = "veryfast";
		std::string profile = "";
		std::string tune = "";
		std::string x264opts = "";
		bool repeat_headers = false;
	};

public:
	X264Encoder();
	virtual ~X264Encoder();
	virtual bool Init();
	virtual bool Update(const char* jsondata);
	virtual void InputRawData(uint8_t** data, size_t* lineSize, int plane, uint64_t pts, bool* receiveFrame);
	virtual void GetEncodedData(BaseEncoder::EncodedFrameData* data);
	virtual void GetEncoderExtraData(uint8_t** data, size_t* size);

private:
	bool SettingsCommit();
	void OutputHeaders();
	bool SetupSettingsPresetTune(const char* preset, const char* tune);
	void ApplyX264Profile(const char* profile);
	void ApplyX264Params();

private:
	X264Param x264_export_param_;
	x264_param_t x264_params_;
	bool init_flag_ = false;
	x264_t* x264_context_ = nullptr;
	uint8_t* extra_data_ = nullptr;
	size_t extra_data_size_ = 0;
	BaseEncoder::EncodedFrameData encoded_frame_;

	long long cur_pts_ = 0;

	uint64_t start_timestamp_ = 0;
	uint64_t fps_interval_ = 0;

};

#endif