#include "media-source-logic.h"
#include "core-engine.h"
#include "core-d3d.h"
#include "json.hpp"
#include "logger.h"

MediaSourceLogic::MediaSourceLogic()
{

}

MediaSourceLogic::~MediaSourceLogic()
{
	media_controler_.reset(nullptr);
}

bool MediaSourceLogic::Init()
{
	return true;
}

bool MediaSourceLogic::Update(const char* jsondata)
{
	if (media_controler_)
		return true;

	try
	{
		auto json = nlohmann::json::parse(jsondata);
		if (!json.is_object())
			return false;
		if (json.find("path") != json.end())
		{
			std::string path = json.at("path");
			media_controler_.reset(new MediaControler(std::bind(&MediaSourceLogic::MediaStopped, this)));
			media_controler_->StartMedia(std::wstring(path.begin(), path.end()).c_str());
			return true;
		}
	}
	catch (...)
	{

	}

	return false;
}

void MediaSourceLogic::MediaStopped()
{

}

void MediaSourceLogic::UpdateTextureSize()
{
	source_texture_size_.width = texture_width_;
	source_texture_size_.height = texture_height_;
}

bool MediaSourceLogic::Tick()
{
	CoreD3D* d3d = core_engine_->GetD3D();

	if (media_controler_ && media_controler_->GetFrameReady())
	{
		int lineSize[4] = { 0 };
		uint8_t* picData[4] = { 0 };
		int width = 0;
		int height = 0;
		media_controler_->GetFrameData(picData, lineSize, width, height);
		if (!m_pTexture)
		{
			d3d->CreateD3DTexture(m_pTexture.GetAddressOf(), false, false, width, height, false);
		}
		if (!m_pResourceView)
		{
			d3d->CreateShaderResourceView(m_pTexture.Get(), m_pResourceView.GetAddressOf());
		}
		texture_width_ = width;
		texture_height_ = height;
		uint8_t* ptr;
		uint32_t linesize_out;
		size_t row_copy;

		D3D11_MAPPED_SUBRESOURCE map;

		d3d->Map(m_pTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);

		ptr = (uint8_t*)map.pData;
		linesize_out = map.RowPitch;
		uint32_t linesize = lineSize[0];
		uint8_t* data = picData[0];
		row_copy = (linesize < linesize_out) ? linesize : linesize_out;
		if (linesize == linesize_out)
		{
			memcpy(ptr, data, row_copy * height);
		}
		else
		{
			uint8_t* const end = ptr + height * linesize_out;
			while (ptr < end) {
				memcpy(ptr, data, row_copy);
				ptr += linesize_out;
				data += linesize;
			}
		}
		d3d->UnMap(m_pTexture.Get(), 0);

		media_controler_->RenderFrameFinish();
	}
	return true;
}

bool MediaSourceLogic::Render()
{
	//if (media_controler_)
	//	media_controler_->RenderAudio();

	if (m_pTexture)
	{
		CoreD3D* d3d = core_engine_->GetD3D();
		d3d->UpdateVertexShader(CoreD3DData::VertexHlslType::kBasic2D);
		d3d->UpdatePixelShader(CoreD3DData::PixelHlslType::kBasic2D);
		d3d->PSSetShaderResources(0, 1, m_pResourceView.GetAddressOf());

		Draw2DSource();
		return true;
	}

	return false;
}