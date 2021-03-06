#pragma once

#include <Windows.h>
#include <stdint.h>
#include <memory>
#include <string>
#include "dx-header.h"
#include "core-d3d-data.h"

class CoreEngine;

class IBaseFilter
{
public:
	virtual ~IBaseFilter() = default;
	void SetCoreEnv(CoreEngine* engine) { core_engine_ = engine; }
	void SetFilterName(const char* name) { filter_name_ = std::string(name); }
	const char* GetFilterName() { return filter_name_.c_str(); }

	virtual bool Init() = 0;
	virtual void SetInputTexture(ID3D11Texture2D* texture, size_t width, size_t height, uint8_t* data = nullptr) = 0;
	virtual void CopyOutputTexture(ID3D11Texture2D* texture) = 0;
	virtual void Update(const char* jsondata) = 0;

protected:
	CoreEngine* core_engine_ = nullptr;
	std::string filter_name_ = "";
};