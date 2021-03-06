#include "base-source-i.h"

IBaseSource::~IBaseSource()
{
	if (core_filter_)
	{
		delete core_filter_;
		core_filter_ = nullptr;
	}
};

void IBaseSource::SetCoreEnv(CoreEngine* engine)
{
	memset(&source_texture_size_, 0, sizeof(source_texture_size_));
	memset(&source_rect_, 0, sizeof(source_rect_));
	memset(&source_rect_type_, 0, sizeof(source_rect_type_));
	core_engine_ = engine;
}

void IBaseSource::SetSourceName(const char* name)
{
	source_name_ = std::string(name);
}

const char* IBaseSource::GetSourceName()
{
	return source_name_.c_str();
}

const IBaseSource::SourceRect* IBaseSource::GetSourceRect()
{
	return &source_rect_;
}

CoreFilter* IBaseSource::GetCoreFilter()
{
	if (!core_filter_)
	{
		core_filter_ = new CoreFilter();
		core_filter_->SetCoreEnv(core_engine_);
	}
	return core_filter_;
}

bool IBaseSource::UpdateEntry(const char* jsondata)
{
	try
	{
		auto json = nlohmann::json::parse(jsondata);
		if (!json.is_object())
			return false;
		if (json.find("topX") != json.end() && json.find("topY") != json.end() && json.find("width") != json.end() && json.find("height") != json.end())
		{
			int topX = std::stoi((std::string)json.at("topX"));
			int topY = std::stoi((std::string)json.at("topY"));
			int width = std::stoi((std::string)json.at("width"));
			int height = std::stoi((std::string)json.at("height"));
			source_rect_type_.topX = topX;
			source_rect_type_.topY = topY;
			source_rect_type_.width = width;
			source_rect_type_.height = height;

			if (source_rect_type_.width)
				source_texture_size_.width = source_rect_type_.width;

			if (source_rect_type_.height)
				source_texture_size_.height = source_rect_type_.height;
		}
		else
		{
			if (source_rect_type_.topX != 0 || source_rect_type_.topY != 0 || source_rect_type_.width != 0 || source_rect_type_.height != 0)
			{

			}
			else
			{
				source_rect_type_.topX = (int32_t)CoreSceneData::AlignType::kAlignLeft;
				source_rect_type_.topY = (int32_t)CoreSceneData::AlignType::kAlignTop;
				source_rect_type_.width = (int32_t)CoreSceneData::SizeType::kClientSize;
				source_rect_type_.height = (int32_t)CoreSceneData::SizeType::kClientSize;
			}
		}
		Update(jsondata);
	}
	catch (...)
	{

	}
	return false;
}

bool IBaseSource::TickEntry()
{
	bool ret = Tick();
	UpdateTextureSize();
	source_rect_.width = (float)source_texture_size_.width;
	source_rect_.height = (float)source_texture_size_.height;
	return ret;
}

void IBaseSource::Draw2DSource()
{
	CoreD3D* d3d = core_engine_->GetD3D();
	UINT buffersize = 0;
	if (GetCoreFilter()->GetFiltersSize())
	{
		auto meshData = Geometry::Create2DShow();
		d3d->ResetMesh(meshData, buffersize);
	}
	else
	{
		float cenx = 0, ceny = 0, scalex = 0, scaley = 0;
		GetRenderSize(cenx, ceny, scalex, scaley);
		auto meshData = Geometry::Create2DShow(cenx, ceny, scalex, scaley);
		d3d->ResetMesh(meshData, buffersize);
	}
	d3d->DrawIndexed(buffersize, 0, 0);
}

void IBaseSource::GetRenderSize(float& cenx, float& ceny, float& scalex, float& scaley)
{
	CoreSettings* settings = core_engine_->GetSettings();
	CoreD3D* d3d = core_engine_->GetD3D();
	D3D11_VIEWPORT port;
	d3d->GetFrontViewPort(&port);
	if (port.Width == 0 && port.Height == 0)
		return;

	float clientWidth = port.Width;
	float clientHeight = port.Height;
	float width = (float)source_texture_size_.width;
	float height = (float)source_texture_size_.height;
	SourceRect rect;

	switch ((CoreSceneData::SizeType)source_rect_type_.width)
	{
	case CoreSceneData::SizeType::kClientSize:
	{
		rect.width = clientWidth;
	}
	break;
	case CoreSceneData::SizeType::kActuallySize:
	{
		rect.width = width;
	}
	break;
	default:
	{
		rect.width = width;
	}
	break;
	}

	switch ((CoreSceneData::SizeType)source_rect_type_.height)
	{
	case CoreSceneData::SizeType::kClientSize:
	{
		rect.height = clientHeight;
	}
	break;
	case CoreSceneData::SizeType::kActuallySize:
	{
		rect.height = height;
	}
	break;
	default:
	{
		rect.height = height;
	}
	break;
	}

	switch ((CoreSceneData::AlignType)source_rect_type_.topX)
	{
	case CoreSceneData::AlignType::kAlignLeft:
	{
		rect.topX = (float)-1.0 + rect.width / clientWidth;
	}
	break;
	case CoreSceneData::AlignType::kAlignRight:
	{
		rect.topX = (float)1.0 - width / clientWidth;
	}
	break;
	default:
	{
		rect.topX = 0;
	}
	break;
	}

	switch ((CoreSceneData::AlignType)source_rect_type_.topY)
	{
	case CoreSceneData::AlignType::kAlignTop:
	{
		rect.topY = (float)1.0 - rect.height / clientHeight;
	}
	break;
	case CoreSceneData::AlignType::kAlignBottom:
	{
		rect.topY = (float)-1.0 + rect.height / clientHeight;
	}
	break;
	default:
	{
		rect.topY = 0;
	}
	break;
	}
	cenx = rect.topX;
	ceny = rect.topY;
	scalex = rect.width / clientWidth;
	scaley = rect.height / clientHeight;
}

