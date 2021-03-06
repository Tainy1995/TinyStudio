#include "logger.h"
#include "core-engine.h"
#include "core-display.h"
#include "core-settings.h"
#include "core-scene.h"
#include "core-video.h"
#include "core-audio.h"
#include "core-d3d.h"
#include "core-output.h"
#include "json.hpp"
#include "platform.h"
#include <string>
#include "fifo_map.hpp"
#include "gdiplus-text-source.h"
#include <codecvt>

template<class K, class V, class dummy_compare, class A>
using my_workaround_fifo_map = nlohmann::fifo_map<K, V, nlohmann::fifo_map_compare<K>, A>;
using unordered_json = nlohmann::basic_json<my_workaround_fifo_map>;

CoreEngine::CoreEngine()
{
	Logger::GetInstance()->InitLogger(false);
	core_display_ = new CoreDisplay();
	core_settings_ = new CoreSettings();
	core_scene_ = new CoreScene();
	core_video_ = new CoreVideo();
	core_audio_ = new CoreAudio();
	core_d3d_ = new CoreD3D();
	core_output_ = new CoreOutput();
	GdiplusTextSource::ModuleLoad();
}

CoreEngine::~CoreEngine()
{
	core_output_->PreEndup();
	core_video_->PreEndup();
	core_audio_->PreEndup();

#define SafeDelete(ptr)	\
				if(ptr)	\
				{			\
					delete ptr;	\
					ptr = nullptr;	\
				}

	SafeDelete(core_output_);
	SafeDelete(core_video_);
	SafeDelete(core_audio_);
	SafeDelete(core_display_);
	SafeDelete(core_scene_);
	SafeDelete(core_d3d_);
	SafeDelete(core_settings_);
	
#undef SafeDelete

	GdiplusTextSource::ModuleUnLoad();
	Logger::GetInstance()->Release();
}

void CoreEngine::SetAppInstance(HINSTANCE hInstance)
{
	app_instance_ = hInstance;
}

int CoreEngine::StartUpCoreEngine()
{
#define CoreInit(ptr)	\
				if(ptr)	\
				{			\
					ptr->SetCoreEnv(this);	\
				}
	CoreInit(core_display_);
	CoreInit(core_scene_);
	CoreInit(core_settings_);
	CoreInit(core_video_);
	CoreInit(core_audio_);
	CoreInit(core_d3d_);
	CoreInit(core_output_);

#undef CoreInit

	DWORD tick = ::GetTickCount();
	std::string outputfilename = std::string(os_get_run_dev_path());
	outputfilename += "\\flv_";
	outputfilename += std::to_string(tick);
	outputfilename += ".flv";

	try
	{
		core_settings_->UpdateGlobalVideo(&(CoreSettingsData::VideoBuilder()
			.width(768)
			.height(432)
			.fps(60)
			.bitrate(2000).video));
		
		core_settings_->UpdateGlobalAudio(&(CoreSettingsData::AudioBuilder()
			.audio_format(CoreAudioData::audio_format::AUDIO_FORMAT_FLOAT_PLANAR)
			.speakers(CoreAudioData::speaker_layout::SPEAKERS_STEREO)
			.samples_per_sec(48000)
			.bitrate(160).audio));

		core_settings_->UpdateGlobalOutput(&(CoreSettingsData::OutputBuilder()
			.enable(false)
			.path(outputfilename.c_str())
			.video(&(CoreSettingsData::VideoBuilder()
				.width(1920)
				.height(1080)
				.fps(60)
				.bitrate(2000).video))
			.audio(&(CoreSettingsData::AudioBuilder()
				.audio_format(CoreAudioData::audio_format::AUDIO_FORMAT_FLOAT_PLANAR)
				.speakers(CoreAudioData::speaker_layout::SPEAKERS_STEREO)
				.samples_per_sec(48000)
				.bitrate(160).audio))
			.output));

		core_display_->StartupCoreDisplay(app_instance_);
		
		core_d3d_->StartupCoreD3DEnv(core_display_->GetMainHwnd());
		
		core_scene_->GenerateSources(GenerateSourcesJson());
		
		core_audio_->StartupCoreAudio();

		core_video_->StartupCoreVideo();

		core_output_->StartupCoreOutput();

		return core_display_->RunLoop();
	}
	catch (const char* error)
	{
		std::string serror = std::string(error);
		std::wstring werror = std::wstring(serror.begin(), serror.end());
		MessageBox(0, werror.c_str(), L"Error", 0);
	}
	return -1;
}

const char* CoreEngine::GenerateSourcesJson()
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> cv;
	std::wstring run_path = std::wstring(os_get_run_dev_wpath());
	std::wstring media_path_1 = run_path + L"\\test1.mp4";
	std::wstring image_path_1 = run_path + L"\\test.png";
	std::wstring model_1 = run_path + L"\\model\\diablo3";
	std::wstring text_1 = L"fps:0";
	std::wstring boltpath = run_path + L"\\BoltAnim";
	
	unordered_json sources = {
		{"sources",
			{
				//{
				//	{"name", "desktop_1"},
				//	{"type", std::to_string((int)CoreSceneData::SourceType::kSourceDesktop)},
				//	{"param", { {"topX", std::to_string((int)CoreSceneData::AlignType::kAlignLeft)},
				//					 {"topY",std::to_string((int)CoreSceneData::AlignType::kAlignTop)},
				//					 {"width", std::to_string((int)CoreSceneData::SizeType::kClientSize)},
				//					 {"height",std::to_string((int)CoreSceneData::SizeType::kClientSize)},
				//	} },
				//},
				{
					{"name", "media_1"},
					{"type", std::to_string((int)CoreSceneData::SourceType::kSourceMedia)},
					{"param", { {"topX", std::to_string((int)CoreSceneData::AlignType::kAlignLeft)},
									 {"topY",std::to_string((int)CoreSceneData::AlignType::kAlignTop)},
									 {"width", std::to_string((int)CoreSceneData::SizeType::kClientSize)},
									 {"height",std::to_string((int)CoreSceneData::SizeType::kClientSize)},
									 {"path", cv.to_bytes(media_path_1)},
					} },
					{"filters", {
						//{
						//	{"name", "filter_1"}, {"type", std::to_string((int)CoreSceneData::FilterType::kFilterFaceDetect)},
						//},
						{
							{"name", "filter_2"}, {"type", std::to_string((int)CoreSceneData::FilterType::kFilterSplit)},
						},
						{
							{"name", "filter_3"}, {"type", std::to_string((int)CoreSceneData::FilterType::kFilterBoltBox)},
							{"param", { {"path", cv.to_bytes(boltpath)}} },
						},
					}},
				},
				{
					{"name", "text_fps"},
					{"type", std::to_string((int)CoreSceneData::SourceType::kSourceGdiplusText)},
					{"param", { {"topX", std::to_string((int)CoreSceneData::AlignType::kAlignRight)},
									 {"topY", std::to_string((int)CoreSceneData::AlignType::kAlignTop)},
									 {"width", std::to_string((int)CoreSceneData::SizeType::kActuallySize)},
									 {"height",std::to_string((int)CoreSceneData::SizeType::kActuallySize)},
									 {"text", cv.to_bytes(text_1)},
					} },
				},
				{
					{"name", "image_1"},
					{"type", std::to_string((int)CoreSceneData::SourceType::kSourceImage)},
					{"param", { {"topX", std::to_string((int)CoreSceneData::AlignType::kAlignLeft)},
									 {"topY",std::to_string((int)CoreSceneData::AlignType::kAlignTop)},
									 {"width", std::to_string((int)CoreSceneData::SizeType::kActuallySize)},
									 {"height",std::to_string((int)CoreSceneData::SizeType::kActuallySize)},
									 {"path", cv.to_bytes(image_path_1)},
					} },
				},
				//{
				//	{"name", "model_1"},
				//	{"type", std::to_string((int)CoreSceneData::SourceType::kSourceObjModel)},
				//	{"param", { {"topX", std::to_string((int)CoreSceneData::AlignType::kAlignLeft)},
				//					 {"topY", std::to_string((int)CoreSceneData::AlignType::kAlignTop)},
				//					 {"width", std::to_string((int)240)},
				//					 {"height",std::to_string((int)320)},
				//					 {"path", cv.to_bytes(model_1)},
				//	} },
				//},
			}
		}
	};

	sources_json_ = sources.dump();
	return sources_json_.c_str();
}