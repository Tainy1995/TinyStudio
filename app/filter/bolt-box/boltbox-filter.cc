#include "boltbox-filter.h"
#include "d3dUtil.h"
#include "DXTrace.h"
#include "Vertex.h"
#include "Geometry.h"
#include "core-d3d.h"
#include "core-engine.h"
#include "json.hpp"

BoltBoxFilter::BoltBoxFilter()
{

}

BoltBoxFilter::~BoltBoxFilter()
{
	for (auto& c = bolt_bitmap_vec_.begin(); c != bolt_bitmap_vec_.end();)
	{
		BitmapTexture::Bitmap* bitmap = *c;
		c = bolt_bitmap_vec_.erase(c);
		delete bitmap;
		bitmap = nullptr;
	}
}

bool BoltBoxFilter::Init()
{
	return true;
}

void BoltBoxFilter::Update(const char* jsondata)
{
	try
	{
		auto json = nlohmann::json::parse(jsondata);
		if (!json.is_object())
			return;
		if (json.find("path") != json.end())
		{
			std::string path = json.at("path");
			InitBoltBitmapRes(path.c_str());
		}
	}
	catch (...)
	{

	}
}

void BoltBoxFilter::SetInputTexture(ID3D11Texture2D* texture, size_t width, size_t height, uint8_t* data)
{
	CoreD3D* d3d = core_engine_->GetD3D();

	ResetBoltResource(width,height);
	LoadBoltResource();
	ResetFilterResource(width, height);

	ComPtr<ID3D11RenderTargetView> View;
	ComPtr<ID3D11DepthStencilView> Depth;
	d3d->OMGetRenderTargets(1, View.GetAddressOf(), Depth.GetAddressOf());

	d3d->CopyResource(output_texture_.Get(), texture);

	d3d->ClearDepthStencilView(depth_stencil_view_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	d3d->OMSetRenderTargets(1, render_target_view_.GetAddressOf(), depth_stencil_view_.Get());

	d3d->UpdateVertexShader(CoreD3DData::VertexHlslType::kBasic2D);
	d3d->UpdatePixelShader(CoreD3DData::PixelHlslType::kBasic2D);

	UINT buffersize = 0;
	auto meshData = Geometry::Create2DShow();
	d3d->ResetMesh(meshData, buffersize);
	d3d->PSSetShaderResources(0, 1, output_texture_view_.GetAddressOf());
	d3d->DrawIndexed(buffersize, 0, 0);

	buffersize = LoadBoltVertexBuffer();
	d3d->PSSetShaderResources(0, 1, bolt_shader_resource_.GetAddressOf());
	d3d->DrawIndexed(buffersize, 0, 0);
	
	d3d->CopyResource(output_texture_.Get(), render_texture_.Get());

	d3d->OMSetRenderTargets(1, View.GetAddressOf(), Depth.Get());

}

void BoltBoxFilter::CopyOutputTexture(ID3D11Texture2D* texture)
{
	CoreD3D* d3d = core_engine_->GetD3D();
	if (output_texture_)
		d3d->CopyResource(texture, output_texture_.Get());
}

void BoltBoxFilter::ResetFilterResource(int width, int height)
{
	CoreD3D* d3d = core_engine_->GetD3D();
	if (texture_width_ != width || texture_height_ != height)
	{
		texture_width_ = width;
		texture_height_ = height;

		output_texture_.Reset();
		output_texture_view_.Reset();
		render_texture_.Reset();
		render_target_view_.Reset();

		d3d->CreateD3DTexture(output_texture_.GetAddressOf(), false, false, width, height, false);
		d3d->CreateShaderResourceView(output_texture_.Get(), output_texture_view_.GetAddressOf());
		d3d->CreateD3DTexture(render_texture_.GetAddressOf(), true, false, width, height, false);
		d3d->CreateRenderTargetView(render_texture_.Get(), render_target_view_.GetAddressOf());
		d3d->CreateDepthStencilBuffer(depth_stencil_buffer_.GetAddressOf(), width, height);
		d3d->CreateDepthStencilView(depth_stencil_buffer_.Get(), depth_stencil_view_.GetAddressOf());
	}
}

void BoltBoxFilter::LoadBoltResource()
{
	
	if (!bolt_texture_)
		return;

	if (bolt_direction_ == BoltDirection::kStart)
		bolt_direction_ = (BoltDirection)(bolt_direction_ + 1);

	CoreD3D* d3d = core_engine_->GetD3D();

	BitmapTexture::Bitmap* bitmap = bolt_bitmap_vec_[cur_bolt_frame_];

	uint8_t* ptr;
	uint32_t linesize_out;
	D3D11_MAPPED_SUBRESOURCE map;
	d3d->Map(bolt_texture_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
	size_t row_copy;
	int width = bitmap->GetW();
	int height = bitmap->GetH();

	ptr = (uint8_t*)map.pData;
	linesize_out = map.RowPitch;
	uint32_t linesize = width * 4;
	uint8_t* data = bitmap->GetBits();
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
	d3d->UnMap(bolt_texture_.Get(), 0);


	cur_bolt_frame_ += 1;
	if (cur_bolt_frame_ >= max_bolt_frame_)
	{
		cur_bolt_frame_ = 0;
		bolt_direction_ = (BoltDirection)(bolt_direction_ + 1);
		if (bolt_direction_ == BoltDirection::kEnd)
			bolt_direction_ = (BoltDirection)(BoltDirection::kStart + 1);
	}
}

void BoltBoxFilter::ResetBoltResource(int width, int height)
{
	if (bolt_bitmap_vec_.empty())
		return;
	CoreD3D* d3d = core_engine_->GetD3D();

	if (!bolt_texture_)
	{
		int width = bolt_bitmap_vec_[0]->GetW();
		int height = bolt_bitmap_vec_[0]->GetH();
		d3d->CreateD3DTexture(bolt_texture_.GetAddressOf(), false, false, width, height, false);
		d3d->CreateShaderResourceView(bolt_texture_.Get(), bolt_shader_resource_.GetAddressOf());
	}

}

void BoltBoxFilter::InitBoltBitmapRes(const char* path)
{
	std::string sPath = path;
	char fix[4] = { 0 };
	for (int i = 1; i <= max_bolt_frame_; i++)
	{
		sprintf_s(fix, "%03d", i);
		fix[3] = '\0';
		std::string filePath = sPath + "\\Bolt" + fix + ".bmp";
		BitmapTexture::Bitmap* bitmap = BitmapTexture::Bitmap::LoadFile(filePath.c_str());
		if (bitmap)
		{
			bolt_bitmap_vec_.push_back(bitmap);
		}
	}

}

UINT BoltBoxFilter::LoadBoltVertexBuffer()
{
	using namespace DirectX;

	CoreD3D* d3d = core_engine_->GetD3D();

	auto DrawSnake = [&](std::vector<VertexPosTex>& vertexvec, std::vector<DWORD>& indexvec, BoltDirection direction, float startpos, float line, DWORD offset) {
		float height = bolt_lineheight_ / 2.0 / (float)texture_height_;
		float width = bolt_lineheight_ / 2.0 / (float)texture_width_;
		switch (direction)
		{
		case BoltDirection::kUp:
		{
			vertexvec.push_back({ XMFLOAT3(startpos, 1.0f - height, 0.0f), XMFLOAT2(0.0f, 1.0f) });
			vertexvec.push_back({ XMFLOAT3(startpos, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) });
			vertexvec.push_back({ XMFLOAT3(startpos + line, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) });
			vertexvec.push_back({ XMFLOAT3(startpos + line, 1.0f - height, 0.0f), XMFLOAT2(1.0f, 1.0f) });
		}
		break;
		case BoltDirection::kRight:
		{
			vertexvec.push_back({ XMFLOAT3(1.0f - width, startpos - line, 0.0f), XMFLOAT2(1.0f, 1.0f) });
			vertexvec.push_back({ XMFLOAT3(1.0f - width, startpos, 0.0f), XMFLOAT2(0.0f, 1.0f) });
			vertexvec.push_back({ XMFLOAT3(1.0f, startpos, 0.0f), XMFLOAT2(0.0f, 0.0f) });
			vertexvec.push_back({ XMFLOAT3(1.0f, startpos - line, 0.0f), XMFLOAT2(1.0f, 0.0f) });
		}
		break;
		case BoltDirection::kDown:
		{
			vertexvec.push_back({ XMFLOAT3(startpos - line, -1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) });
			vertexvec.push_back({ XMFLOAT3(startpos - line, -1.0f + height, 0.0f), XMFLOAT2(1.0f, 1.0f) });
			vertexvec.push_back({ XMFLOAT3(startpos, -1.0f + height, 0.0f), XMFLOAT2(0.0f, 1.0f) });
			vertexvec.push_back({ XMFLOAT3(startpos, -1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) });
		}
		break;
		case BoltDirection::kLeft:
		{
			vertexvec.push_back({ XMFLOAT3(-1.0f, startpos, 0.0f), XMFLOAT2(0.0f, 0.0f) });
			vertexvec.push_back({ XMFLOAT3(-1.0f, startpos + line, 0.0f), XMFLOAT2(1.0f, 0.0f) });
			vertexvec.push_back({ XMFLOAT3(-1.0f + width, startpos + line, 0.0f), XMFLOAT2(1.0f, 1.0f) });
			vertexvec.push_back({ XMFLOAT3(-1.0f + width, startpos, 0.0f), XMFLOAT2(0.0f, 1.0f) });
		}
		break;
		}
		indexvec.insert(indexvec.end(), { offset,offset + 1, offset + 2, offset + 2, offset + 3, offset });
	};

	vertex_buffer_.Reset();
	index_buffer_.Reset();

	float total = (texture_width_ + texture_height_) * 2.0;
	float boltline = texture_height_ > texture_width_ ? texture_height_ : texture_width_;
	float step = total / (float)max_bolt_frame_ ;
	float walked = step * cur_step_cnt_;
	cur_step_cnt_ += 1;
	if (cur_step_cnt_ >= max_bolt_frame_ )
		cur_step_cnt_ = 0;

	std::vector<VertexPosTex> vertexvec;
	std::vector< DWORD> indexvec;

	if (walked < texture_width_)
	{
		float linewalked = walked;

		if (linewalked >= boltline)
		{
			float startpos = (linewalked - boltline) / texture_width_ * 2.0 - 1.0;
			float line = boltline / texture_width_ * 2.0;
			DrawSnake(vertexvec, indexvec, BoltDirection::kUp, startpos, line, 0);
		}
		else
		{
			float startpos = (1.0 - (boltline - linewalked) / texture_height_) * 2.0 - 1.0;
			float line = (boltline - linewalked) / texture_height_ * 2.0;
			DrawSnake(vertexvec, indexvec, BoltDirection::kLeft, startpos, line, 0);

			startpos = -1.0;
			line = linewalked / texture_width_ * 2.0;
			DrawSnake(vertexvec, indexvec, BoltDirection::kUp, startpos, line, vertexvec.size());
		}
	}
	else if (walked >= texture_width_ && walked < (texture_width_ + texture_height_))
	{
		float linewalked = walked - texture_width_;

		if (linewalked >= boltline)
		{
			float startpos = (1.0 - (linewalked - boltline) / texture_height_) * 2.0 - 1.0;
			float line = boltline / texture_height_ * 2.0;
			DrawSnake(vertexvec, indexvec, BoltDirection::kRight, startpos, line, 0);
		}
		else
		{
			float startpos = 1.0;
			float line = linewalked / texture_height_ * 2.0;
			DrawSnake(vertexvec, indexvec, BoltDirection::kRight, startpos, line, 0);

			startpos = (1.0 - (boltline - linewalked) / texture_width_ ) * 2.0 - 1.0;
			line = (boltline - linewalked) / texture_width_ * 2.0;
			DrawSnake(vertexvec, indexvec, BoltDirection::kUp, startpos, line, vertexvec.size());
		}
	}
	else if (walked >= (texture_width_ + texture_height_) && walked < (texture_width_ * 2 + texture_height_))
	{
		float linewalked = walked - texture_width_ - texture_height_;
		if (linewalked >= boltline)
		{
			float startpos = (1.0 - (linewalked - boltline) / texture_width_) * 2.0 - 1.0;
			float line = boltline / texture_width_ * 2.0;
			DrawSnake(vertexvec, indexvec, BoltDirection::kDown, startpos, line, 0);
		}
		else
		{
			float startpos = 1.0;
			float line = linewalked / texture_width_ * 2.0;
			DrawSnake(vertexvec, indexvec, BoltDirection::kDown, startpos, line, 0);

			startpos = (boltline - linewalked) / texture_height_ * 2.0 - 1.0;
			line = (boltline - linewalked) / texture_height_ * 2.0;
			DrawSnake(vertexvec, indexvec, BoltDirection::kRight, startpos, line, vertexvec.size());
		}
	}
	else
	{
		float linewalked = walked - texture_width_* 2 - texture_height_;
		if (linewalked >= boltline)
		{
			float startpos = (linewalked - boltline) / texture_height_ * 2.0 - 1.0;
			float line = boltline / texture_height_ * 2.0;
			DrawSnake(vertexvec, indexvec, BoltDirection::kLeft, startpos, line, 0);
		}
		else
		{
			float startpos = -1.0;
			float line = linewalked / texture_height_ * 2.0;
			DrawSnake(vertexvec, indexvec, BoltDirection::kLeft, startpos, line, 0);

			startpos = (boltline - linewalked) / texture_width_ * 2.0 - 1.0;
			line = (boltline - linewalked) / texture_width_ * 2.0;
			DrawSnake(vertexvec, indexvec, BoltDirection::kDown, startpos, line, vertexvec.size());
		}
	}
	
	UINT index_buffer_size_ = 0;
	

	D3D11_BUFFER_DESC vbd;
	ZeroMemory(&vbd, sizeof(vbd));
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = (UINT)vertexvec.size() * (sizeof(VertexPosTex));
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = vertexvec.data();
	HR(d3d->GetD3DDevice()->CreateBuffer(&vbd, &InitData, vertex_buffer_.GetAddressOf()));

	UINT stride = sizeof(VertexPosTex);
	UINT offset = 0;
	d3d->GetD3DDeviceContext()->IASetVertexBuffers(0, 1, vertex_buffer_.GetAddressOf(), &stride, &offset);

	index_buffer_size_ = (UINT)indexvec.size();
	D3D11_BUFFER_DESC ibd;
	ZeroMemory(&ibd, sizeof(ibd));
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(DWORD) * index_buffer_size_;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	InitData.pSysMem = indexvec.data();
	HR(d3d->GetD3DDevice()->CreateBuffer(&ibd, &InitData, index_buffer_.GetAddressOf()));
	d3d->GetD3DDeviceContext()->IASetIndexBuffer(index_buffer_.Get(), DXGI_FORMAT_R32_UINT, 0);

	return index_buffer_size_;
}