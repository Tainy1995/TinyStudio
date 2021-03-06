#include "core-scene.h"
#include "json.hpp"
#include "desktop-capture.h"
#include "core-display.h"
#include "core-engine.h"
#include "core-d3d.h"
#include "core-filter.h"
#include "media-source-logic.h"
#include "gdiplus-text-source.h"
#include "split-filter.h"
#include "obj-model-source.h"
#include "boltbox-filter.h"
#include "image-source.h"
#include "face-detect/facedetect-filter.h"

CoreScene::CoreScene()
{

}

CoreScene::~CoreScene()
{
	for (auto& c = source_list_.begin(); c != source_list_.end();)
	{
		IBaseSource* source = *c;
		c = source_list_.erase(c);
		delete source;
		source = nullptr;
	}
}

IBaseSource* CoreScene::FindNamedSource(std::string sourcename)
{
	IBaseSource* ptr = nullptr;
	for (const auto& c : source_list_)
	{
		if (c->GetSourceName() == sourcename)
		{
			ptr = c;
			return ptr;
		}
	}
	return ptr;
}

void CoreScene::GenerateSources(const char* jsondata)
{
	try
	{
		auto json = nlohmann::json::parse(jsondata);
		const auto sJson = json.at("sources");
		if (!sJson.is_array())
			return;

		for (size_t i = 0; i < sJson.size(); i++)
		{
			const auto& c = sJson[i];
			if (c.find("name") != c.end() && c.find("type") != c.end())
			{
				std::string name = c.at("name");
				std::string type = c.at("type");
				AllocNewSources(name, (CoreSceneData::SourceType)(std::stoi(type)));
				if (c.find("param") != c.end())
				{
					std::string paramjson = c.at("param").dump();
					UpdateSourceProperty(c.at("name"), paramjson.c_str());
				}
				if (c.find("filters") != c.end())
				{
					std::string filterjson = c.at("filters").dump();
					GenerateFilters(name, filterjson.c_str());
				}
			}
		}
	}
	catch(...)
	{

	}
}

void CoreScene::GenerateFilters(std::string sourcename, const char* jsondata)
{
	try
	{
		auto json = nlohmann::json::parse(jsondata);
		if (!json.is_array())
			return;
		for (size_t i = 0; i < json.size(); i++)
		{
			const auto& c = json[i];
			if (c.find("name") != c.end() && c.find("type") != c.end())
			{
				std::string name = c.at("name");
				std::string type = c.at("type");
				AllocNewFilter(sourcename, name, (CoreSceneData::FilterType)(std::stoi(type)));
				if (c.find("param") != c.end())
				{
					std::string paramjson = c.at("param").dump();
					UpdateFilterProperty(sourcename, name, paramjson.c_str());
				}
			}
		}
	}
	catch (...)
	{

	}
}

void CoreScene::AllocNewFilter(std::string sourcename, std::string filtername, CoreSceneData::FilterType filtertype)
{
	if (!FindNamedSource(sourcename))
		return;

	IBaseSource* source = FindNamedSource(sourcename);
	CoreFilter* core_filter = source->GetCoreFilter();

	IBaseFilter* filter = nullptr;
	switch (filtertype)
	{
	case CoreSceneData::FilterType::kFilterFaceDetect:
	{
		filter = new FaceDetectFilter();
	}
	break;
	case CoreSceneData::FilterType::kFilterSplit:
	{
		filter = new SplitFilter();
	}
	break;
	case CoreSceneData::FilterType::kFilterBoltBox:
	{
		filter = new BoltBoxFilter();
	}
	break;
	}
	if (!filter)
		return;

	filter->SetCoreEnv(core_engine_);
	if (filter->Init())
	{
		core_filter->AppendFilter(filtername, filter);
	}
	else
	{
		delete filter;
		filter = nullptr;
	}
}

void CoreScene::TickSources()
{
	for (const auto& c : source_list_)
	{
		c->TickEntry();
	}
}

void CoreScene::RenderSources()
{
	for (const auto& c : source_list_)
	{

		c->GetCoreFilter()->TextureFilters(c);
	}
}

void CoreScene::AllocNewSources(std::string name, CoreSceneData::SourceType type)
{
	if (FindNamedSource(name))
		return;

	CoreD3D* d3d = core_engine_->GetD3D();

	IBaseSource* source = nullptr;
	switch (type)
	{
	case CoreSceneData::SourceType::kSourceDesktop:
	{
		source = new DesktopCaptureSource();
	}
	break;
	case CoreSceneData::SourceType::kSourceMedia:
	{
		source = new MediaSourceLogic();
	}
	break;
	case CoreSceneData::SourceType::kSourceGdiplusText:
	{
		source = new GdiplusTextSource();
	}
	break;
	case CoreSceneData::SourceType::kSourceObjModel:
	{
		source = new ObjModelSource();
	}
	break;
	case CoreSceneData::SourceType::kSourceImage:
	{
		source = new ImageSource();
	}
	break;
	}
	if (!source)
		return;

	source->SetCoreEnv(core_engine_);
	source->SetSourceName(name.c_str());
	if (source->Init())
	{
		source_list_.push_back(source);
	}
	else
	{
		delete source;
		source = nullptr;
	}
}

void CoreScene::UpdateFilterProperty(std::string sourcename, std::string filtername, const char* json)
{
	if (!FindNamedSource(sourcename))
		return;

	IBaseSource* source = FindNamedSource(sourcename);
	CoreFilter* core_filter = source->GetCoreFilter();
	IBaseFilter* filter = core_filter->GetNamedFilter(filtername);
	if (!filter)
		return;
	filter->Update(json);
}

void CoreScene::UpdateSourceProperty(std::string name, const char* json)
{
	if (!FindNamedSource(name))
		return;

	IBaseSource* source = FindNamedSource(name);
	source->UpdateEntry(json);
}