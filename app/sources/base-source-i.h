#pragma once

#include "core-filter.h"
#include <Windows.h>
#include <memory>
#include <wrl/client.h>
#include <string>
#include <d3d11_1.h>
#include <DirectXMath.h>
#include "core-d3d-data.h"
#include "json.hpp"
#include "core-scene-data.h"
#include "core-settings.h"
#include "core-engine.h"
#include "core-d3d.h"

class CoreEngine;

class IBaseSource
{
public:
	struct SourceTextureSize
	{
		int32_t width;
		int32_t height;
	};

	struct SourceRect
	{
		float topX;
		float topY;
		float width;
		float height;
	};

private:
	struct SourceRectType
	{
		int32_t topX;
		int32_t topY;
		int32_t width;
		int32_t height;
	};

public:
	virtual ~IBaseSource();
	void SetCoreEnv(CoreEngine* engine);
	void SetSourceName(const char* name);
	const char* GetSourceName();
	const SourceRect* GetSourceRect();
	CoreFilter* GetCoreFilter();
	bool UpdateEntry(const char* jsondata);
	bool TickEntry();
	void GetRenderSize(float& cenx, float& ceny, float& scalex, float& scaley);
	virtual bool Init() = 0;
	virtual bool Render() = 0;
	
protected:
	virtual bool Update(const char* json) = 0;
	virtual bool Tick() = 0;
	virtual void UpdateTextureSize() = 0;
	void Draw2DSource();

protected:
	CoreEngine* core_engine_ = nullptr;
	CoreFilter* core_filter_ = nullptr;
	std::string source_name_ = "";

	SourceTextureSize source_texture_size_;
	SourceRect source_rect_;
	SourceRectType source_rect_type_;
};