#include "core-settings.h"

CoreSettings::CoreSettings()
{
	global_output_.path = output_path_.c_str();
}

CoreSettings::~CoreSettings()
{

}

void CoreSettings::UpdateGlobalVideo(const CoreSettingsData::Video* video)
{
	std::unique_lock<std::mutex> lock(global_param_mutex_);
	memcpy(&global_video_, video, sizeof(CoreSettingsData::Video));
}

void CoreSettings::UpdateGlobalAudio(const CoreSettingsData::Audio* audio)
{
	std::unique_lock<std::mutex> lock(global_param_mutex_);
	memcpy(&global_audio_, audio, sizeof(CoreSettingsData::Audio));
}

void CoreSettings::UpdateGlobalOutput(const CoreSettingsData::Output* output)
{
	std::unique_lock<std::mutex> lock(global_param_mutex_);
	output_path_ = std::string(output->path);
	global_output_.enable = output->enable;
	global_output_.path = output_path_.c_str();
	memcpy(&global_output_.video, &output->video, sizeof(CoreSettingsData::Video));
	memcpy(&global_output_.audio, &output->audio, sizeof(CoreSettingsData::Audio));
}

const CoreSettingsData::Video* CoreSettings::GetVideoParam()
{
	std::unique_lock<std::mutex> lock(global_param_mutex_);
	return &global_video_;
}

const CoreSettingsData::Audio* CoreSettings::GetAudioParam()
{
	std::unique_lock<std::mutex> lock(global_param_mutex_);
	return &global_audio_;
}

const CoreSettingsData::Output* CoreSettings::GetOutputParam()
{
	std::unique_lock<std::mutex> lock(global_param_mutex_);
	return &global_output_;
}