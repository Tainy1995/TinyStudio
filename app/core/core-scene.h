#ifndef CORE_SCENE_H
#define CORE_SCENE_H

#include "core-component-i.h"
#include "core-scene-data.h"
#include "base-source-i.h"
#include <map>
#include <string>
#include <unordered_map>
#include <list>

class CoreScene : public ICoreComponent
{
public:
	CoreScene();
	virtual ~CoreScene();

	void GenerateSources(const char* jsondata);

	void TickSources();
	void RenderSources();
	void UpdateSourceProperty(std::string name, const char* json);
	void UpdateFilterProperty(std::string sourcename, std::string filtername, const char* json);

private:
	void AllocNewSources(std::string name, CoreSceneData::SourceType type);
	void GenerateFilters(std::string sourcename, const char* jsondata);
	void AllocNewFilter(std::string sourcename, std::string filtername, CoreSceneData::FilterType filtertype);
	IBaseSource* FindNamedSource(std::string sourcename);

private:
	std::list<IBaseSource*> source_list_;
};

#endif