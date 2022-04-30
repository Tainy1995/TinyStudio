#include "image-source.h"
#include "core-engine.h"
#include "core-d3d.h"
#include "json.hpp"
#include "logger.h"
#include "graphics-ffmpeg.h"

ImageSource::ImageSource()
{

}

ImageSource::~ImageSource()
{
	gs_free_image_deps();
}

bool ImageSource::Init()
{
	gs_init_image_deps();
	return true;
}

bool ImageSource::Update(const char* jsondata)
{
	try
	{
		auto json = nlohmann::json::parse(jsondata);
		if (!json.is_object())
			return false;
		if (json.find("path") != json.end())
		{
			std::string path = json.at("path");
			image_file_path_ = path;
			return true;
		}
	}
	catch (...)
	{

	}

	return false;
}

bool ImageSource::Render()
{
	if (m_pTexture)
	{
		CoreD3D* d3d = core_engine_->GetD3D();
		d3d->UpdateVertexShader(CoreD3DData::VertexHlslType::kBasic2D);
		d3d->UpdatePixelShader(CoreD3DData::PixelHlslType::kBasic2D);
		d3d->PSSetShaderResources(0, 1, m_pResourceView.GetAddressOf());
		d3d->OpenAlphaBlend();
		Draw2DSource();
		d3d->CloseAlphaBlend();
		return true;
	}

	return false;
}

bool ImageSource::Tick()
{
	if (!image_file_path_.empty())
	{
		CoreD3D* d3d = core_engine_->GetD3D();

		uint32_t width = 0;
		uint32_t height = 0;
		int size = 0;
		gs_color_format format;
		uint8_t* data = gs_create_texture_file_data(image_file_path_.c_str(), &format, &width, &height, size);
		if (!data)
		{
			image_file_path_.clear();
			return false;
		}

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
		uint32_t linesize = width * 4;
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
		free(data);
		image_file_path_.clear();
		return true;
	}
	return true;
}

void ImageSource::UpdateTextureSize()
{
	source_texture_size_.width = texture_width_;
	source_texture_size_.height = texture_height_;
}