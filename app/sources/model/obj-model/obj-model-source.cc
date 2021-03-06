#include "obj-model-source.h"
#include "core-d3d.h"
#include "core-engine.h"

ObjModelSource::ObjModelSource()
{

}

ObjModelSource::~ObjModelSource()
{
	if (obj_render_mgr_)
	{
		delete obj_render_mgr_;
		obj_render_mgr_ = nullptr;
	}
}

bool ObjModelSource::Init()
{
	return true;
}

bool ObjModelSource::Render()
{

	if (source_texture_size_.width != pre_width_ || source_texture_size_.height != pre_height_)
	{
		if (!InitRenderTexture())
			return false;
	}

	CoreD3D* d3d = core_engine_->GetD3D();

	ComPtr<ID3D11RenderTargetView> View;
	ComPtr<ID3D11DepthStencilView> Depth;
	d3d->OMGetRenderTargets(1, View.GetAddressOf(), Depth.GetAddressOf());

	static float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	d3d->ClearRenderTargetView(render_targetview_.Get(), color);
	d3d->ClearDepthStencilView(depth_stencil_view_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	d3d->OMSetRenderTargets(1, render_targetview_.GetAddressOf(), depth_stencil_view_.Get());
	d3d->PushFrontViewPort((float)source_texture_size_.width, (float)source_texture_size_.height);

	if (obj_render_mgr_)
		obj_render_mgr_->ForeachDrawFaces();
	
	d3d->PopFrontViewPort();

	d3d->OMSetRenderTargets(1, View.GetAddressOf(), Depth.Get());
	d3d->UpdateVertexShader(CoreD3DData::VertexHlslType::kBasic2D);
	d3d->UpdatePixelShader(CoreD3DData::PixelHlslType::kBasic2D);
	d3d->OpenAlphaBlend();
	float cenx = 0, ceny = 0, scalex = 0, scaley = 0;
	GetRenderSize(cenx, ceny, scalex, scaley);
	UINT buffersize = 0;
	auto meshData = Geometry::Create2DShow(cenx, ceny, scalex, scaley);
	d3d->ResetMesh(meshData, buffersize);
	d3d->PSSetShaderResources(0, 1, render_shaderresource_.GetAddressOf());
	d3d->DrawIndexed(buffersize, 0, 0);
	d3d->CloseAlphaBlend();

	return true;
}

bool ObjModelSource::Update(const char* jsondata)
{
	if (obj_render_mgr_)
		return true;

	obj_render_mgr_ = new ObjRenderMgr();
	obj_render_mgr_->SetCoreEngine(core_engine_);

	try
	{
		auto json = nlohmann::json::parse(jsondata);
		if (!json.is_object())
			return false;
		if (json.find("path") != json.end())
		{
			std::string path = json.at("path");
			return obj_render_mgr_->InitObjResourceFolder(path.c_str());
		}
	}
	catch (...)
	{

	}

	return false;
}

bool ObjModelSource::Tick()
{
	return true;
}

void ObjModelSource::UpdateTextureSize()
{

}

bool ObjModelSource::InitRenderTexture()
{
	render_texture_.Reset();
	render_shaderresource_.Reset();
	render_targetview_.Reset();
	depth_stencil_buffer_.Reset();
	depth_stencil_view_.Reset();

	if (!source_texture_size_.width || !source_texture_size_.height)
		return false;

	pre_width_ = source_texture_size_.width;
	pre_height_ = source_texture_size_.height;

	CoreD3D* d3d = core_engine_->GetD3D();
	if (!d3d->CreateD3DTexture(render_texture_.GetAddressOf(), true, false, source_texture_size_.width, source_texture_size_.height, false))
		return false;

	if (!d3d->CreateShaderResourceView(render_texture_.Get(), render_shaderresource_.GetAddressOf()))
		return false;

	if (!d3d->CreateRenderTargetView(render_texture_.Get(), render_targetview_.GetAddressOf()))
		return false;

	if (!d3d->CreateDepthStencilBuffer(depth_stencil_buffer_.GetAddressOf(), source_texture_size_.width, source_texture_size_.height))
		return false;

	if (!d3d->CreateDepthStencilView(depth_stencil_buffer_.Get(), depth_stencil_view_.GetAddressOf()))
		return false;

	return true;
}