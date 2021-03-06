#ifndef CORE_SETTINGS_H
#define CORE_SETTINGS_H

#include "core-settings-data.h"
#include "core-component-i.h"
#include <mutex>
#include <string>

class CoreSettings : public ICoreComponent
{
public:
	CoreSettings();
	virtual ~CoreSettings();
	void UpdateGlobalVideo(const CoreSettingsData::Video* video);
	void UpdateGlobalAudio(const CoreSettingsData::Audio* audio);
	void UpdateGlobalOutput(const CoreSettingsData::Output* output);
	const CoreSettingsData::Video* GetVideoParam();
	const CoreSettingsData::Audio* GetAudioParam();
	const CoreSettingsData::Output* GetOutputParam();

private:
	std::mutex global_param_mutex_;
	CoreSettingsData::Video global_video_;
	CoreSettingsData::Audio global_audio_;
	CoreSettingsData::Output global_output_;

	std::string output_path_ = "";

};

#endif