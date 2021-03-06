#pragma once

/*
* 渲染管理器，管理加载obj资源文件然后初始化和维护渲染相关的顶点，坐标等数据
*/

#include <string>
#include "obj-resource-mgr.h"

class CoreEngine;

class ObjRenderMgr
{
	struct VSConstantBuffer
	{
		DirectX::XMMATRIX world;
		DirectX::XMMATRIX view;
		DirectX::XMMATRIX proj;
	};

public:
	ObjRenderMgr();
	~ObjRenderMgr();

	void SetCoreEngine(CoreEngine* engine) { core_engine_ = engine; }
	bool InitObjResourceFolder(const char* path);
	void ForeachDrawFaces();

private:
	bool InitObjVertext();
	bool InitDiffuseTexture();
	bool InitVSConstantBuffer();

private:
	CoreEngine* core_engine_ = nullptr;
	ObjResourceMgr* obj_res_mgr_ = nullptr;

	ComPtr<ID3D11Buffer> vertex_buffer_;
	ComPtr<ID3D11Buffer>  index_buffer_;

	ComPtr< ID3D11Texture2D> diffuse_texture_;
	ComPtr<ID3D11ShaderResourceView> diffuse_shaderresource_;

	VSConstantBuffer m_VSConstantBuffer;
	ComPtr<ID3D11Buffer> m_pConstantBuffers;
};