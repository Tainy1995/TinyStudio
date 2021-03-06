#include "split-filter.h"
#include "d3dUtil.h"
#include "DXTrace.h"
#include "Vertex.h"
#include "Geometry.h"
#include "core-d3d.h"
#include "core-engine.h"

SplitFilter::SplitFilter()
{

}

SplitFilter::~SplitFilter()
{

}

bool SplitFilter::Init()
{
	return true;
}

void SplitFilter::Update(const char* jsondata)
{

}

void SplitFilter::ResetFilterResource(int width, int height)
{
	CoreD3D* d3d = core_engine_->GetD3D();
	if (texture_width_ != width || texture_height_ != height)
	{
		texture_width_ = width;
		texture_height_ = height;

		output_texture_.Reset();
		output_texture_view_.Reset();
		scale_render_texture_.Reset();
		scale_render_target_.Reset();

		d3d->CreateD3DTexture(output_texture_.GetAddressOf(), false, false, width, height, false);
		d3d->CreateShaderResourceView(output_texture_.Get(), output_texture_view_.GetAddressOf());
		d3d->CreateD3DTexture(scale_render_texture_.GetAddressOf(), true, false, width, height, false);
		d3d->CreateRenderTargetView(scale_render_texture_.Get(), scale_render_target_.GetAddressOf());
	}
}

void SplitFilter::SetInputTexture(ID3D11Texture2D* texture, size_t width, size_t height, uint8_t* data)
{
	CoreD3D* d3d = core_engine_->GetD3D();
	ResetFilterResource(width, height);

	ComPtr<ID3D11RenderTargetView> View;
	ComPtr<ID3D11DepthStencilView> Depth;
	d3d->OMGetRenderTargets(1, View.GetAddressOf(), Depth.GetAddressOf());

	d3d->CopyResource(output_texture_.Get(), texture);

	auto meshData = Geometry::Create2DShow();
	UINT buffersize = 0;
	d3d->ResetMesh(meshData, buffersize);

	d3d->OMSetRenderTargets(1, scale_render_target_.GetAddressOf(), nullptr);

	d3d->UpdateVertexShader(CoreD3DData::VertexHlslType::kBasic2D);
	d3d->UpdatePixelShader(CoreD3DData::PixelHlslType::kThreeMirror2D);

	d3d->PSSetShaderResources(0, 1, output_texture_view_.GetAddressOf());
	d3d->DrawIndexed(buffersize, 0, 0);

	d3d->CopyResource(output_texture_.Get(), scale_render_texture_.Get());

	d3d->OMSetRenderTargets(1, View.GetAddressOf(), Depth.Get());
}

void SplitFilter::CopyOutputTexture(ID3D11Texture2D* texture)
{
	CoreD3D* d3d = core_engine_->GetD3D();
	if(output_texture_)
		d3d->CopyResource(texture, output_texture_.Get());
}