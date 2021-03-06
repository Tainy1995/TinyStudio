#include "core-scene.h"
#include "core-filter.h"
#include "json.hpp"
#include "core-display.h"
#include "core-engine.h"
#include "core-d3d.h"
#include "logger.h"
#include "base-source-i.h"

CoreFilter::CoreFilter()
{

}

CoreFilter::~CoreFilter()
{
	for (auto& c = filter_list_.begin(); c != filter_list_.end();)
	{
		IBaseFilter* source = *c;
		c = filter_list_.erase(c);
		delete source;
		source = nullptr;
	}
}

IBaseFilter* CoreFilter::GetNamedFilter(std::string name)
{
	std::unique_lock<std::mutex> lock(filter_mutex_);
	for (const auto& c : filter_list_)
	{
		if (c->GetFilterName() == name)
			return c;
	}
	return nullptr;
}

void CoreFilter::AppendFilter(std::string name, IBaseFilter* filter)
{
	if (GetNamedFilter(name))
		return;
	std::unique_lock<std::mutex> lock(filter_mutex_);
	filter->SetFilterName(name.c_str());
	filter_list_.push_back(filter);
}

void CoreFilter::RenderSourcesOnly(IBaseSource* source)
{
	source->Render();
}

void CoreFilter::RenderSourcesWidthFilters(IBaseSource* source)
{
	CoreD3D* d3d = core_engine_->GetD3D();

	size_t sourceWidth = (size_t)source->GetSourceRect()->width;
	size_t sourceHeight = (size_t)source->GetSourceRect()->height;
	ComPtr<ID3D11RenderTargetView> View;
	ComPtr<ID3D11DepthStencilView> Depth;
	UINT viewnum = 0;
	uint8_t* copy_ptr = nullptr;
	static float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	ResetFilterResource(sourceWidth, sourceHeight);
	d3d->OMGetRenderTargets(1, View.GetAddressOf(), Depth.GetAddressOf());
	d3d->ClearRenderTargetView(filter_target_view_.Get(), color);
	d3d->ClearDepthStencilView(filter_depth_stencil_view_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	d3d->OMSetRenderTargets(1, filter_target_view_.GetAddressOf(), filter_depth_stencil_view_.Get());

	d3d->PushFrontViewPort((float)sourceWidth, (float)sourceHeight);

	source->Render();

	{
		std::unique_lock<std::mutex> lock(filter_mutex_);
		for (const auto& c : filter_list_)
		{
			c->SetInputTexture(filter_render_texture_.Get(), sourceWidth, sourceHeight, copy_ptr);
			c->CopyOutputTexture(filter_render_texture_.Get());
		}
	}

	d3d->PopFrontViewPort();

	d3d->CopyResource(filer_final_render_texture_.Get(), filter_render_texture_.Get());
	d3d->OMSetRenderTargets(1, View.GetAddressOf(), Depth.Get());
	d3d->UpdateVertexShader(CoreD3DData::VertexHlslType::kBasic2D);
	d3d->UpdatePixelShader(CoreD3DData::PixelHlslType::kBasic2D);

	float cenx = 0, ceny = 0, scalex = 0, scaley = 0;
	source->GetRenderSize(cenx, ceny, scalex, scaley);
	UINT buffersize = 0;
	auto meshData = Geometry::Create2DShow(cenx, ceny, scalex, scaley);
	d3d->ResetMesh(meshData, buffersize);
	d3d->PSSetShaderResources(0, 1, filter_final_render_view_.GetAddressOf());
	d3d->DrawIndexed(buffersize, 0, 0);
}

void CoreFilter::TextureFilters(IBaseSource* source)
{
	if (!source)
		return;

	size_t sourceWidth = (size_t)source->GetSourceRect()->width;
	size_t sourceHeight = (size_t)source->GetSourceRect()->height;

	if (sourceWidth == 0 && sourceHeight == 0)
		return;

	bool renderFilters = false;

	{
		std::unique_lock<std::mutex> lock(filter_mutex_);
		if (filter_list_.size())
			renderFilters = true;
	}

	if (renderFilters)
		RenderSourcesWidthFilters(source);
	else
		RenderSourcesOnly(source);
}

size_t CoreFilter::GetFiltersSize()
{
	std::unique_lock<std::mutex> lock(filter_mutex_);
	return filter_list_.size();
}

void CoreFilter::ResetFilterResource(size_t sourceWidth, size_t sourceHeight)
{
	CoreD3D* d3d = core_engine_->GetD3D();

	bool reset = false;
	if (texture_width_ != sourceWidth || texture_height_ != sourceHeight)
		reset = true;

	if (reset)
	{
		filter_render_texture_.Reset();
		filter_target_view_.Reset();
		filter_depth_stencil_buffer_.Reset();
		filter_depth_stencil_view_.Reset();

		texture_width_ = sourceWidth;
		texture_height_ = sourceHeight;

		if (!d3d->CreateD3DTexture(filter_render_texture_.GetAddressOf(), true, false, sourceWidth, sourceHeight, false))
			EXCEPTION_TEXT("filter render texture failed!");

		if (!d3d->CreateRenderTargetView(filter_render_texture_.Get(), filter_target_view_.GetAddressOf()))
			EXCEPTION_TEXT("filter render target view failed!");

		if (!d3d->CreateD3DTexture(filer_final_render_texture_.GetAddressOf(), false, false, sourceWidth, sourceHeight, false))
			EXCEPTION_TEXT("filter copy texture failed!");

		if(!d3d->CreateShaderResourceView(filer_final_render_texture_.Get(), filter_final_render_view_.GetAddressOf()))
			EXCEPTION_TEXT("filter copy view failed!");

		if (!d3d->CreateDepthStencilBuffer(filter_depth_stencil_buffer_.GetAddressOf(), sourceWidth, sourceHeight))
			EXCEPTION_TEXT("filter depth stencil buffer failed!");

		if (!d3d->CreateDepthStencilView(filter_depth_stencil_buffer_.Get(), filter_depth_stencil_view_.GetAddressOf()))
			EXCEPTION_TEXT("filter depth stencil view failed!");
	}
}
