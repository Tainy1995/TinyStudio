#include "x264-encoder.h"
#include "logger.h"
#include "core-settings.h"
#include "core-engine.h"

static const char* astrblank = "";
static int astrcmpi(const char* str1, const char* str2)
{
	if (!str1)
		str1 = astrblank;
	if (!str2)
		str2 = astrblank;

	do {
		char ch1 = (char)toupper(*str1);
		char ch2 = (char)toupper(*str2);

		if (ch1 < ch2)
			return -1;
		else if (ch1 > ch2)
			return 1;
	} while (*str1++ && *str2++);

	return 0;
}

static void log_x264(void* param, int level, const char* format, va_list args)
{
}

static inline int get_x264_cs_val(const char* const name,
	const char* const names[])
{
	int idx = 0;
	do {
		if (strcmp(names[idx], name) == 0)
			return idx;
	} while (!!names[++idx]);

	return 0;
}

static const char* validate( const char* val, const char* name, const char* const* list)
{
	if (!val || !*val)
		return val;

	while (*list) {
		if (strcmp(val, *list) == 0)
			return val;

		list++;
	}

	return NULL;
}

static inline const char* validate_preset(const char* preset)
{
	const char* new_preset = validate(preset, "preset", x264_preset_names);
	return new_preset ? new_preset : "veryfast";
}

static inline void init_pic_data(int i_csp, int64_t pts,  uint8_t** data, size_t* linesize, x264_picture_t* pic)
{
	x264_picture_init(pic);

	pic->i_pts = pts;
	pic->img.i_csp = i_csp;
	pic->img.i_plane = 3;
	for (int i = 0; i < pic->img.i_plane; i++) {
		pic->img.i_stride[i] = (int)linesize[i];
		pic->img.plane[i] = data[i];
	}
}

X264Encoder::X264Encoder()
{

}

X264Encoder::~X264Encoder()
{
	if (x264_context_) 
	{
		x264_encoder_close(x264_context_);
		x264_context_ = nullptr;
	}
	if (extra_data_)
	{
		free(extra_data_);
		extra_data_ = nullptr;
	}
	if (encoded_frame_.data)
	{
		free(encoded_frame_.data);
		encoded_frame_.data = nullptr;
	}
}

bool X264Encoder::Init()
{
	if (SettingsCommit())
	{
		x264_context_ = x264_encoder_open(&x264_params_);
		if (x264_context_)
		{
			OutputHeaders();
			init_flag_ = true;
			return true;
		}
	}
	return false;
}

void X264Encoder::OutputHeaders()
{
	auto pushArray = [&](std::vector<uint8_t>& list, uint8_t* data, size_t size)
	{
		for (size_t i = 0; i < size; i++)
		{
			list.push_back(data[i]);
		}
	};

	x264_nal_t* nals;
	int nal_count;
	std::vector<uint8_t> header;
	std::vector<uint8_t> sei;

	x264_encoder_headers(x264_context_, &nals, &nal_count);

	for (int i = 0; i < nal_count; i++) {
		x264_nal_t* nal = nals + i;

		if (nal->i_type == NAL_SEI)
			pushArray(sei, nal->p_payload, nal->i_payload);
		else
			pushArray(header, nal->p_payload, nal->i_payload);
	}

	extra_data_size_ = header.size();
	extra_data_ = (uint8_t*)malloc(header.size());
	memset(extra_data_, 0, header.size());
	memcpy(extra_data_, &header[0], header.size());

	LOGGER_INFO("x264 header size:%d", header.size());
}

bool X264Encoder::Update(const char* jsondata)
{
	//parse json to fill x264_param_
	if (!init_flag_)
		return false;

	if (SettingsCommit()) 
	{
		int ret = x264_encoder_reconfig(x264_context_, &x264_params_);
		return ret == 0;
	}

	return false;
}

bool X264Encoder::SettingsCommit()
{
	bool success = true;
	if (!x264_context_)
	{
		success = SetupSettingsPresetTune(x264_export_param_.preset.c_str(), x264_export_param_.tune.c_str());
	}

	if (x264_export_param_.repeat_headers) {
		x264_params_.b_repeat_headers = 1;
		x264_params_.b_annexb = 1;
		x264_params_.b_aud = 1;
	}

	if (success)
	{
		ApplyX264Params();

		if (!x264_context_)
		{
			ApplyX264Profile(x264_export_param_.profile.c_str());
		}
	}
	
	return success;
}

void X264Encoder::InputRawData(uint8_t** data, size_t* lineSize, int plane, uint64_t pts, bool* receiveFrame)
{
	*receiveFrame = false;
	if (!init_flag_)
		return;

	if (!start_timestamp_)
		start_timestamp_ = pts;

	if (!fps_interval_)
	{
		CoreSettings* settings = core_engine_->GetSettings();
		int fps = settings->GetVideoParam()->fps;
		double intervalms = 1000.0 / fps;
		fps_interval_ = (uint64_t)((uint64_t)1000000.0 * intervalms);
	}

	uint64_t now_pts = 0;
	if (fps_interval_)
	{
		now_pts = (pts - start_timestamp_) / fps_interval_;
	}
	if (now_pts < cur_pts_)
		now_pts = cur_pts_;

	auto pushArray = [&](std::vector<uint8_t>& list, uint8_t* data, size_t size)
	{
		for (size_t i = 0; i < size; i++)
		{
			list.push_back(data[i]);
		}
	};

	x264_nal_t* nals = nullptr;
	int nal_count = 0;;
	int ret;
	x264_picture_t pic, pic_out;

	init_pic_data(x264_params_.i_csp, now_pts, data, lineSize, &pic);

	ret = x264_encoder_encode(x264_context_, &nals, &nal_count, &pic, &pic_out);
	if (ret < 0) {
		*receiveFrame = false;
		return;
	}

	*receiveFrame = (nal_count != 0);
	if (*receiveFrame)
	{
		if (encoded_frame_.data)
		{
			free(encoded_frame_.data);
			encoded_frame_.data = nullptr;
		}
		std::vector<uint8_t> framedata;

		for (int i = 0; i < nal_count; i++) {
			x264_nal_t* nal = nals + i;
			pushArray(framedata, nal->p_payload, nal->i_payload);
		}

		encoded_frame_.size = framedata.size();
		encoded_frame_.data = (uint8_t*)malloc(framedata.size());
		memcpy(encoded_frame_.data, &framedata[0], framedata.size());
		encoded_frame_.pts = pic_out.i_pts;
		encoded_frame_.dts = pic_out.i_dts;
		encoded_frame_.keyframe = pic_out.b_keyframe != 0;
		encoded_frame_.bAudio = false;

		//LOGGER_INFO("now_pts:%lld encoded pts:%lld", now_pts, encoded_frame_.pts);
	}

	cur_pts_ += 1;
}

void X264Encoder::GetEncodedData(BaseEncoder::EncodedFrameData* data)
{
	*data = encoded_frame_;
}

void X264Encoder::GetEncoderExtraData(uint8_t** data, size_t* size)
{
	if (!extra_data_ || !extra_data_size_)
		return;

	*size = extra_data_size_;
	*data = (uint8_t*)malloc(extra_data_size_);
	memset(*data, 0, extra_data_size_);
	memcpy(*data, extra_data_, extra_data_size_);
}

bool X264Encoder::SetupSettingsPresetTune(const char* preset, const char* tune)
{
	int ret = x264_param_default_preset(&x264_params_, validate_preset(preset), validate(tune, "tune", x264_tune_names));
	return ret == 0;
}

void X264Encoder::ApplyX264Profile(const char* profile)
{
	if (x264_context_)
		return;

	if (profile && *profile) {
		x264_param_apply_profile(&x264_params_, profile);
	}
}

enum rate_control {
	RATE_CONTROL_CBR,
	RATE_CONTROL_VBR,
	RATE_CONTROL_ABR,
	RATE_CONTROL_CRF
};

void X264Encoder::ApplyX264Params()
{
	CoreSettings* settings = core_engine_->GetSettings();
	const char* rate_control = x264_export_param_.rate_control.c_str();
	int bitrate = x264_export_param_.bitrate;
	int buffer_size = x264_export_param_.buffer_size;
	int keyint_sec = x264_export_param_.keyint_sec;
	int crf = x264_export_param_.crf;
	int width = settings->GetOutputParam()->video.width;
	int height = settings->GetOutputParam()->video.height;
	int bf = 0;
	bool use_bufsize = x264_export_param_.use_bufsize;
	enum rate_control rc;

	if (astrcmpi(rate_control, "ABR") == 0) {
		rc = RATE_CONTROL_ABR;
		crf = 0;

	}
	else if (astrcmpi(rate_control, "VBR") == 0) {
		rc = RATE_CONTROL_VBR;

	}
	else if (astrcmpi(rate_control, "CRF") == 0) {
		rc = RATE_CONTROL_CRF;
		bitrate = 0;
		buffer_size = 0;

	}
	else { /* CBR */
		rc = RATE_CONTROL_CBR;
		crf = 0;
	}

	if (keyint_sec)
		x264_params_.i_keyint_max = keyint_sec * settings->GetOutputParam()->video.fps;

	if (!use_bufsize)
		buffer_size = bitrate;

	x264_params_.b_vfr_input = false;
	x264_params_.rc.i_vbv_max_bitrate = bitrate;
	x264_params_.rc.i_vbv_buffer_size = buffer_size;
	x264_params_.rc.i_bitrate = bitrate;
	x264_params_.i_width = width;
	x264_params_.i_height = height;
	x264_params_.i_fps_num = settings->GetOutputParam()->video.fps;
	x264_params_.i_fps_den = 1;
	x264_params_.i_timebase_num = 1;
	x264_params_.i_timebase_den = settings->GetOutputParam()->video.fps;
	x264_params_.pf_log = log_x264;
	x264_params_.p_log_private = nullptr;
	x264_params_.i_log_level = X264_LOG_WARNING;

	static const char* const smpte170m = "smpte170m";
	static const char* const bt709 = "bt709";
	static const char* const iec61966_2_1 = "iec61966-2-1";
	const char* colorprim = NULL;
	const char* transfer = NULL;
	const char* colmatrix = NULL;
	colorprim = bt709;
	transfer = bt709;
	colmatrix = bt709;

	x264_params_.vui.i_sar_height = 1;
	x264_params_.vui.i_sar_width = 1;
	x264_params_.vui.b_fullrange = true;
	x264_params_.vui.i_colorprim = get_x264_cs_val(colorprim, x264_colorprim_names);
	x264_params_.vui.i_transfer = get_x264_cs_val(transfer, x264_transfer_names);
	x264_params_.vui.i_colmatrix = get_x264_cs_val(colmatrix, x264_colmatrix_names);

	/* use the new filler method for CBR to allow real-time adjusting of
	 * the bitrate */
	if (rc == RATE_CONTROL_CBR || rc == RATE_CONTROL_ABR) {
		x264_params_.rc.i_rc_method = X264_RC_ABR;

		if (rc == RATE_CONTROL_CBR) {
#if X264_BUILD >= 139
			x264_params_.rc.b_filler = true;
#else
			obsx264->params.i_nal_hrd = X264_NAL_HRD_CBR;
#endif
		}
	}
	else {
		x264_params_.rc.i_rc_method = X264_RC_CRF;
		x264_params_.rc.f_rf_constant = (float)crf;
	}
	x264_params_.i_csp = X264_CSP_I420;

}