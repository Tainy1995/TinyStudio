#ifndef CORE_ENGINE_H
#define CORE_ENGINE_H

#include <Windows.h>
#include <memory>
#include <string>


class CoreDisplay;
class CoreSettings;
class CoreScene;
class CoreVideo;
class CoreAudio;
class CoreD3D;
class CoreOutput;

class CoreEngine
{
public:
	CoreEngine();
	~CoreEngine();
	void SetAppInstance(HINSTANCE hInstance);
	int StartUpCoreEngine();
	CoreDisplay* GetDisplay() { return core_display_; }
	CoreSettings* GetSettings() { return core_settings_; }
	CoreScene* GetScene() { return core_scene_; }
	CoreD3D* GetD3D() { return core_d3d_; }
	CoreVideo* GetVideo() { return core_video_; }
	CoreAudio* GetAudio() { return core_audio_; }
	CoreOutput* GetOutput() { return core_output_; }

private:
	const char* GenerateSourcesJson();

private:
	CoreDisplay* core_display_ = nullptr;
	CoreSettings* core_settings_ = nullptr;
	CoreScene* core_scene_ = nullptr;
	CoreVideo* core_video_ = nullptr;
	CoreAudio* core_audio_ = nullptr;
	CoreD3D* core_d3d_ = nullptr;
	CoreOutput* core_output_ = nullptr;
	HINSTANCE app_instance_ = 0;
	std::string sources_json_ = "";
};

#endif