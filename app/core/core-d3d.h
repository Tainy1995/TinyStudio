#ifndef CORE_D3D_H
#define CORE_D3D_H

#include <memory>
#include <string>
#include <Windows.h>
#include <map>
#include "core-component-i.h"
#include "dx-header.h"
#include "core-d3d-data.h"
#include <list>

class CoreD3D : public ICoreComponent
{
public:
	CoreD3D();
	virtual ~CoreD3D();
	void StartupCoreD3DEnv(HWND hwnd);

	void WindowResized();
	void RenderBegin();
	void RenderEnd();
	void RenderOutputTextureBegin();
	void RenderOutputTextureEnd(uint8_t** bgra_data, size_t& linesize);

	void UpdateVertexShader(CoreD3DData::VertexHlslType type);
	void UpdatePixelShader(CoreD3DData::PixelHlslType type);

	ComPtr<ID3D11Device> GetD3DDevice() { return m_pd3dDevice; }
	ComPtr<ID3D11DeviceContext> GetD3DDeviceContext() { return m_pd3dImmediateContext; }
	ComPtr<IDXGISwapChain> GetD3DSwapChain() { return m_pSwapChain; }
	ComPtr<IDXGIAdapter> GetD3DAdapter() { return m_pAdapter; }

	bool CreateD3DTexture(ID3D11Texture2D** texture, bool bRenderTarget, bool gdi, int width, int height, bool bRead);
	bool CreateShaderResourceView(ID3D11Texture2D* texture, ID3D11ShaderResourceView** resourceView);
	bool CreateRenderTargetView(ID3D11Texture2D* texture, ID3D11RenderTargetView** renderView);
	bool CreateDepthStencilBuffer(ID3D11Texture2D** texture, int width, int height);
	bool CreateDepthStencilView(ID3D11Texture2D* texture, ID3D11DepthStencilView** view);

	bool ResetMesh(const Geometry::MeshData<VertexPosTex, DWORD>& meshData, UINT& bufferSize);
	void PSSetShaderResources(int startslots, int numviews, ID3D11ShaderResourceView** view);
	void DrawIndexed(UINT indexcount, UINT startlocation, UINT baselocation);
	void Map(ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource);
	void UnMap(ID3D11Resource* pResource, UINT Subresource);
	void CopyResource(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource);
	void OMGetRenderTargets(UINT NumViews, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView);
	void OMSetRenderTargets(UINT NumViews, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView);
	void ClearRenderTargetView( ID3D11RenderTargetView* pRenderTargetView, const FLOAT ColorRGBA[4]);
	void ClearDepthStencilView(ID3D11DepthStencilView* pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil);
	void OpenAlphaBlend();
	void CloseAlphaBlend();

	void PushFrontViewPort(float width, float height);
	void GetFrontViewPort(D3D11_VIEWPORT* port);
	void PopFrontViewPort();

private:
	void InitD3D(HWND hwnd);
	void InitHLSLShaderResource();
	void InitD3DResource();
	void InitOutputResource();
	void InitDepthStencilState();
	void InitAlphaBlendState();

private:
	ComPtr<ID3D11Device> m_pd3dDevice;                    
	ComPtr<ID3D11DeviceContext> m_pd3dImmediateContext;  
	ComPtr<IDXGISwapChain> m_pSwapChain;                  
	ComPtr< IDXGIAdapter> m_pAdapter;
	ComPtr<ID3D11Device1> m_pd3dDevice1;
	ComPtr<ID3D11DeviceContext1> m_pd3dImmediateContext1;
	ComPtr<IDXGISwapChain1> m_pSwapChain1;
	ComPtr< IDXGIAdapter1> m_pAdapter1;
	bool	  m_Enable4xMsaa = false;
	UINT      m_4xMsaaQuality = 0;

	ComPtr<ID3D11Texture2D> m_pDepthStencilBuffer;
	ComPtr<ID3D11RenderTargetView> m_pRenderTargetView;
	ComPtr<ID3D11DepthStencilView> m_pDepthStencilView;
	ComPtr<ID3D11InputLayout> vertex_layout_;
	ComPtr<ID3D11InputLayout> model_vetex_layout_;
	ComPtr<ID3D11Buffer> vertex_buffer_;
	ComPtr<ID3D11Buffer> index_buffer_;
	ComPtr<ID3D11Buffer> constant_buffer_[2];
	ComPtr<ID3D11SamplerState> sampler_state_;
	ComPtr< ID3D11DepthStencilState> no_depth_stencil_state_;
	ComPtr< ID3D11BlendState> alpha_blend_state_;
	UINT index_buffer_size_ = 0;

	ComPtr< ID3D11Texture2D> output_render_texture_;
	ComPtr< ID3D11RenderTargetView> output_target_view_;
	ComPtr< ID3D11Texture2D> output_copy_texture_;
	ComPtr<ID3D11Texture2D> output_depth_stencil_buffer_;
	ComPtr<ID3D11DepthStencilView> output_depth_stencil_view_;

	std::map<CoreD3DData::VertexHlslType, ID3D11VertexShader*> vertex_shader_map_;
	std::map<CoreD3DData::PixelHlslType, ID3D11PixelShader*> pixel_shader_map_;

	std::list< D3D11_VIEWPORT > viewport_list_;
};

#endif