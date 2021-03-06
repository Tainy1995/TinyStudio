#pragma once

#include "base-source-i.h"
#include "dx-header.h"
#include "obj-render-mgr.h"

class ObjModelSource : public IBaseSource
{
public:
	ObjModelSource();
	virtual ~ObjModelSource();

	virtual bool Init();
	virtual bool Render();

protected:
	virtual bool Update(const char* jsondata);
	virtual bool Tick();
	virtual void UpdateTextureSize();
	bool InitRenderTexture();

private:
	ObjRenderMgr* obj_render_mgr_ = nullptr;

	ComPtr< ID3D11Texture2D> render_texture_;
	ComPtr<ID3D11ShaderResourceView> render_shaderresource_;
	ComPtr<ID3D11RenderTargetView> render_targetview_;
	ComPtr<ID3D11Texture2D> depth_stencil_buffer_;
	ComPtr<ID3D11DepthStencilView> depth_stencil_view_;

	int pre_width_ = 0;
	int pre_height_ = 0;
};