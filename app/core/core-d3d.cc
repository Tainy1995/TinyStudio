#include "core-d3d.h"
#include "logger.h"
#include "core-engine.h"
#include "core-settings.h"
#include "platform.h"

CoreD3D::CoreD3D()
{

}

CoreD3D::~CoreD3D()
{
	for (auto& iter = vertex_shader_map_.begin(); iter != vertex_shader_map_.end(); )
	{
		auto& p = iter->second;
		p->Release();
		p = nullptr;
		iter = vertex_shader_map_.erase(iter);
	}

	for (auto& iter = pixel_shader_map_.begin(); iter != pixel_shader_map_.end(); )
	{
		auto& p = iter->second;
		p->Release();
		p = nullptr;
		iter = pixel_shader_map_.erase(iter);
	}
}

void CoreD3D::StartupCoreD3DEnv(HWND hwnd)
{
	InitD3D(hwnd);
	WindowResized();
	InitHLSLShaderResource();
	InitD3DResource();
	InitOutputResource();
	InitDepthStencilState();
	InitAlphaBlendState();
}

void CoreD3D::InitD3D(HWND hwnd)
{
	
	CoreSettings* setting = core_engine_->GetSettings();
	int width = setting->GetVideoParam()->width;
	int height = setting->GetVideoParam()->height;

	HRESULT hr = S_OK;

	UINT createDeviceFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	createDeviceFlags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);
	
	D3D_FEATURE_LEVEL featureLevel;
	D3D_DRIVER_TYPE d3dDriverType;
	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		d3dDriverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDevice(nullptr, d3dDriverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, m_pd3dDevice.GetAddressOf(), &featureLevel, m_pd3dImmediateContext.GetAddressOf());

		if (hr == E_INVALIDARG)
		{
			hr = D3D11CreateDevice(nullptr, d3dDriverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
				D3D11_SDK_VERSION, m_pd3dDevice.GetAddressOf(), &featureLevel, m_pd3dImmediateContext.GetAddressOf());
		}

		if (SUCCEEDED(hr))
			break;
	}

	if (FAILED(hr))
		EXCEPTION_TEXT("D3D11CreateDevice Failed");

	if (featureLevel != D3D_FEATURE_LEVEL_11_0 && featureLevel != D3D_FEATURE_LEVEL_11_1)
		EXCEPTION_TEXT("Direct3D Feature Level 11 unsupported");

	m_pd3dDevice->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &m_4xMsaaQuality);
	assert(m_4xMsaaQuality > 0);


	ComPtr<IDXGIDevice> dxgiDevice = nullptr;
	ComPtr<IDXGIAdapter> dxgiAdapter = nullptr;
	ComPtr<IDXGIFactory1> dxgiFactory1 = nullptr;	
	ComPtr<IDXGIFactory2> dxgiFactory2 = nullptr;

	HR_EXCEPTION(m_pd3dDevice.As(&dxgiDevice));
	HR_EXCEPTION(dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf()));
	HR_EXCEPTION(dxgiAdapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(dxgiFactory1.GetAddressOf())));

	hr = dxgiFactory1.As(&dxgiFactory2);
	if (dxgiFactory2 != nullptr)
	{
		HR_EXCEPTION(m_pd3dDevice.As(&m_pd3dDevice1));
		HR_EXCEPTION(m_pd3dImmediateContext.As(&m_pd3dImmediateContext1));
		DXGI_SWAP_CHAIN_DESC1 sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.Width = width;
		sd.Height = height;
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		if (m_Enable4xMsaa)
		{
			sd.SampleDesc.Count = 4;
			sd.SampleDesc.Quality = m_4xMsaaQuality - 1;
		}
		else
		{
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
		}
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = 1;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		sd.Flags = 0;

		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fd;
		fd.RefreshRate.Numerator = 60;
		fd.RefreshRate.Denominator = 1;
		fd.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		fd.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		fd.Windowed = TRUE;

		HR_EXCEPTION(dxgiFactory2->CreateSwapChainForHwnd(m_pd3dDevice.Get(), hwnd, &sd, &fd, nullptr, m_pSwapChain1.GetAddressOf()));
		HR_EXCEPTION(m_pSwapChain1.As(&m_pSwapChain));
		HR_EXCEPTION(dxgiFactory2->EnumAdapters1(0, m_pAdapter1.GetAddressOf()));
		HR_EXCEPTION(m_pAdapter1.As(&m_pAdapter));
	}
	else
	{
		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferDesc.Width = width;
		sd.BufferDesc.Height = height;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		if (m_Enable4xMsaa)
		{
			sd.SampleDesc.Count = 4;
			sd.SampleDesc.Quality = m_4xMsaaQuality - 1;
		}
		else
		{
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
		}
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = 1;
		sd.OutputWindow = hwnd;
		sd.Windowed = TRUE;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		sd.Flags = 0;
		HR_EXCEPTION(dxgiFactory1->CreateSwapChain(m_pd3dDevice.Get(), &sd, m_pSwapChain.GetAddressOf()));
		HR_EXCEPTION(dxgiFactory1->EnumAdapters(0, m_pAdapter.GetAddressOf()));
	}
}

void CoreD3D::WindowResized()
{
	if (!m_pd3dDevice)
		return;

	CoreSettings* setting = core_engine_->GetSettings();
	int width = setting->GetVideoParam()->width;
	int height = setting->GetVideoParam()->height;

	if (!m_pd3dImmediateContext || !m_pSwapChain)
		return;

	if (m_pd3dDevice1 != nullptr)
	{
		assert(m_pd3dImmediateContext1);
		assert(m_pd3dDevice1);
		assert(m_pSwapChain1);
	}

	m_pRenderTargetView.Reset();
	m_pDepthStencilView.Reset();
	m_pDepthStencilBuffer.Reset();

	ComPtr<ID3D11Texture2D> backBuffer;
	HR_EXCEPTION(m_pSwapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0));
	HR_EXCEPTION(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf())));
	HR_EXCEPTION(m_pd3dDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, m_pRenderTargetView.GetAddressOf()));

	backBuffer.Reset();

	D3D11_TEXTURE2D_DESC depthStencilDesc;

	depthStencilDesc.Width = width;
	depthStencilDesc.Height = height;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	if (m_Enable4xMsaa)
	{
		depthStencilDesc.SampleDesc.Count = 4;
		depthStencilDesc.SampleDesc.Quality = m_4xMsaaQuality - 1;
	}
	else
	{
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.SampleDesc.Quality = 0;
	}

	depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthStencilDesc.CPUAccessFlags = 0;
	depthStencilDesc.MiscFlags = 0;

	HR_EXCEPTION(m_pd3dDevice->CreateTexture2D(&depthStencilDesc, nullptr, m_pDepthStencilBuffer.GetAddressOf()));
	HR_EXCEPTION(m_pd3dDevice->CreateDepthStencilView(m_pDepthStencilBuffer.Get(), nullptr, m_pDepthStencilView.GetAddressOf()));
	m_pd3dImmediateContext->OMSetRenderTargets(1, m_pRenderTargetView.GetAddressOf(), m_pDepthStencilView.Get());
}

void CoreD3D::RenderBegin()
{
	CoreSettings* settings = core_engine_->GetSettings();
	PushFrontViewPort((float)settings->GetVideoParam()->width, (float)settings->GetVideoParam()->height);

	if (!m_pd3dImmediateContext || !m_pSwapChain)
		return;
	m_pd3dImmediateContext->OMSetDepthStencilState(no_depth_stencil_state_.Get(), 0);
	static float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	m_pd3dImmediateContext->ClearRenderTargetView(m_pRenderTargetView.Get(), color);
	m_pd3dImmediateContext->ClearDepthStencilView(m_pDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	m_pd3dImmediateContext->OMSetRenderTargets(1, m_pRenderTargetView.GetAddressOf(), m_pDepthStencilView.Get());
}

void CoreD3D::RenderOutputTextureBegin()
{
	CoreSettings* settings = core_engine_->GetSettings();

	PushFrontViewPort((float)settings->GetOutputParam()->video.width, (float)settings->GetOutputParam()->video.height);

	static float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_pd3dImmediateContext->ClearRenderTargetView(output_target_view_.Get(), color);
	m_pd3dImmediateContext->ClearDepthStencilView(output_depth_stencil_view_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	m_pd3dImmediateContext->OMSetRenderTargets(1, output_target_view_.GetAddressOf(), output_depth_stencil_view_.Get());
}

void CoreD3D::RenderOutputTextureEnd(uint8_t** bgra_data, size_t& linesize)
{
	PopFrontViewPort();
	m_pd3dImmediateContext->CopyResource(output_copy_texture_.Get(), output_render_texture_.Get());
	D3D11_MAPPED_SUBRESOURCE map;
	HR_EXCEPTION(m_pd3dImmediateContext->Map(output_copy_texture_.Get(), 0, D3D11_MAP_READ, 0, &map));
	char* texture_data = (char*)map.pData;
	int dataSize = map.DepthPitch;

	*bgra_data = (uint8_t*)malloc(dataSize);
	memcpy(*bgra_data, texture_data, dataSize);

	m_pd3dImmediateContext->Unmap(output_copy_texture_.Get(), 0);

	linesize = dataSize;
}

void CoreD3D::RenderEnd()
{
	PopFrontViewPort();

	if (!m_pSwapChain)
		return;

	m_pSwapChain->Present(0, 0);
}

void CoreD3D::InitD3DResource()
{
	ComPtr<ID3DBlob> blob;
	std::wstring rundev = std::wstring(os_get_run_dev_wpath());
	HR_EXCEPTION(CreateShaderFromFile(nullptr, std::wstring(rundev + L"\\HLSL\\Basic_VS_2D.hlsl").c_str(), "VS_2D", "vs_5_0", 
		blob.ReleaseAndGetAddressOf()));
	HR_EXCEPTION(m_pd3dDevice->CreateInputLayout(VertexPosTex::inputLayout, ARRAYSIZE(VertexPosTex::inputLayout),
		blob->GetBufferPointer(), blob->GetBufferSize(), vertex_layout_.GetAddressOf()));

	HR_EXCEPTION(CreateShaderFromFile(nullptr, std::wstring(rundev + L"\\HLSL\\Model_VS.hlsl").c_str(), "VS_2D", "vs_5_0",
		blob.ReleaseAndGetAddressOf()));
	HR_EXCEPTION(m_pd3dDevice->CreateInputLayout(VertexPosNormalTex::inputLayout, ARRAYSIZE(VertexPosNormalTex::inputLayout),
		blob->GetBufferPointer(), blob->GetBufferSize(), model_vetex_layout_.GetAddressOf()));

	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	HR(m_pd3dDevice->CreateSamplerState(&sampDesc, sampler_state_.GetAddressOf()));

	m_pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pd3dImmediateContext->IASetInputLayout(vertex_layout_.Get());
	m_pd3dImmediateContext->PSSetSamplers(0, 1, sampler_state_.GetAddressOf());
}

void CoreD3D::InitAlphaBlendState()
{
	D3D11_BLEND_DESC bd;
	memset(&bd, 0, sizeof(bd));
	bd.AlphaToCoverageEnable = false;
	bd.IndependentBlendEnable = false;
	for (int i = 0; i < 8; i++) {
		bd.RenderTarget[i].BlendEnable = true;
		bd.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[i].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		bd.RenderTarget[i].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		bd.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
		bd.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO;
		bd.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	}

	HR_EXCEPTION(m_pd3dDevice->CreateBlendState(&bd, alpha_blend_state_.GetAddressOf()));
}

void CoreD3D::InitDepthStencilState()
{
	D3D11_DEPTH_STENCIL_DESC dsd;
	memset(&dsd, 0, sizeof(D3D11_DEPTH_STENCIL_DESC));
	dsd.DepthEnable = false;
	HR_EXCEPTION(m_pd3dDevice->CreateDepthStencilState(&dsd, no_depth_stencil_state_.GetAddressOf()));
}

void CoreD3D::InitOutputResource()
{
	CoreSettings* settings = core_engine_->GetSettings();
	int width = settings->GetOutputParam()->video.width;
	int height = settings->GetOutputParam()->video.height;
	if (!CreateD3DTexture(output_render_texture_.GetAddressOf(), true, false, width, height, false))
		EXCEPTION_TEXT("output render texture failed!");

	if (!CreateRenderTargetView(output_render_texture_.Get(), output_target_view_.GetAddressOf()))
		EXCEPTION_TEXT("output render target view failed!");

	if (!CreateD3DTexture(output_copy_texture_.GetAddressOf(), false, false, width, height, true))
		EXCEPTION_TEXT("output copy texture failed!");

	CreateDepthStencilBuffer(output_depth_stencil_buffer_.GetAddressOf(), width, height);
	CreateDepthStencilView(output_depth_stencil_buffer_.Get(), output_depth_stencil_view_.GetAddressOf());
}

void CoreD3D::InitHLSLShaderResource()
{
	ComPtr<ID3DBlob> blob;
	std::wstring rundev = std::wstring(os_get_run_dev_wpath());
	auto CreateVertexShader = [&](CoreD3DData::VertexHlslType type, const wchar_t* filename)
	{
		ID3D11VertexShader* vertex_shader = nullptr;
		HR_EXCEPTION(CreateShaderFromFile(nullptr, std::wstring(rundev + L"\\HLSL\\" + std::wstring(filename)).c_str(), "VS_2D", "vs_5_0", blob.ReleaseAndGetAddressOf()));
		HR_EXCEPTION(m_pd3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vertex_shader));
		if (vertex_shader)
		{
			vertex_shader_map_.insert(std::make_pair(type, vertex_shader));
		}
	};

	auto CreatePixelShader = [&](CoreD3DData::PixelHlslType type, const wchar_t* filename)
	{
		ID3D11PixelShader* pixel_shader = nullptr;
		HR_EXCEPTION(CreateShaderFromFile(nullptr, std::wstring(rundev + L"\\HLSL\\" + std::wstring(filename)).c_str(), "PS_2D", "ps_5_0", blob.ReleaseAndGetAddressOf()));
		HR_EXCEPTION(m_pd3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &pixel_shader));
		if (pixel_shader)
		{
			pixel_shader_map_.insert(std::make_pair(type, pixel_shader));
		}
	};

	CreateVertexShader(CoreD3DData::VertexHlslType::kBasic2D, L"Basic_VS_2D.hlsl");
	CreateVertexShader(CoreD3DData::VertexHlslType::kExpand2D, L"Expand_VS_2D.hlsl");
	CreateVertexShader(CoreD3DData::VertexHlslType::kScale2D, L"Scale_VS_2D.hlsl");
	CreateVertexShader(CoreD3DData::VertexHlslType::kModel3D, L"Model_VS.hlsl");

	CreatePixelShader(CoreD3DData::PixelHlslType::kBasic2D, L"Basic_PS_2D.hlsl");	
	CreatePixelShader(CoreD3DData::PixelHlslType::kAlpha2D, L"Basic_PS_Alpha_2D.hlsl");
	CreatePixelShader(CoreD3DData::PixelHlslType::kModel3D, L"Model_PS.hlsl");
	CreatePixelShader(CoreD3DData::PixelHlslType::kThreeMirror2D, L"Basic_PS_ThreeMirror.hlsl");

}

void CoreD3D::UpdateVertexShader(CoreD3DData::VertexHlslType type)
{
	if (vertex_shader_map_.find(type) == vertex_shader_map_.end())
	{
		m_pd3dImmediateContext->VSSetShader(nullptr, nullptr, 0);
	}
	else
	{
		m_pd3dImmediateContext->VSSetShader(vertex_shader_map_[type], nullptr, 0);
		if (type == CoreD3DData::VertexHlslType::kModel3D)
		{
			m_pd3dImmediateContext->IASetInputLayout(model_vetex_layout_.Get());
		}
		else
		{
			m_pd3dImmediateContext->IASetInputLayout(vertex_layout_.Get());
		}
	}
}

void CoreD3D::UpdatePixelShader(CoreD3DData::PixelHlslType type)
{
	if (pixel_shader_map_.find(type) == pixel_shader_map_.end())
	{
		m_pd3dImmediateContext->PSSetShader(nullptr, nullptr, 0);
	}
	else
	{
		m_pd3dImmediateContext->PSSetShader(pixel_shader_map_[type], nullptr, 0);
	}
}

bool CoreD3D::ResetMesh(const Geometry::MeshData<VertexPosTex, DWORD>& meshData, UINT& bufferSize)
{
	vertex_buffer_.Reset();
	index_buffer_.Reset();

	D3D11_BUFFER_DESC vbd;
	ZeroMemory(&vbd, sizeof(vbd));
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = (UINT)meshData.vertexVec.size() * (sizeof(VertexPosTex));
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = meshData.vertexVec.data();
	HR(m_pd3dDevice->CreateBuffer(&vbd, &InitData, vertex_buffer_.GetAddressOf()));

	UINT stride = sizeof(VertexPosTex);
	UINT offset = 0;
	m_pd3dImmediateContext->IASetVertexBuffers(0, 1, vertex_buffer_.GetAddressOf(), &stride, &offset);

	index_buffer_size_ = (UINT)meshData.indexVec.size();
	bufferSize = index_buffer_size_;
	D3D11_BUFFER_DESC ibd;
	ZeroMemory(&ibd, sizeof(ibd));
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(DWORD) * index_buffer_size_;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	InitData.pSysMem = meshData.indexVec.data();
	HR(m_pd3dDevice->CreateBuffer(&ibd, &InitData, index_buffer_.GetAddressOf()));
	m_pd3dImmediateContext->IASetIndexBuffer(index_buffer_.Get(), DXGI_FORMAT_R32_UINT, 0);

	return true;
}

bool CoreD3D::CreateDepthStencilBuffer(ID3D11Texture2D** texture, int width, int height)
{
	D3D11_TEXTURE2D_DESC depthStencilDesc;
	depthStencilDesc.Width = width;
	depthStencilDesc.Height = height;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	if (m_Enable4xMsaa)
	{
		depthStencilDesc.SampleDesc.Count = 4;
		depthStencilDesc.SampleDesc.Quality = m_4xMsaaQuality - 1;
	}
	else
	{
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.SampleDesc.Quality = 0;
	}

	depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthStencilDesc.CPUAccessFlags = 0;
	depthStencilDesc.MiscFlags = 0;

	HR_EXCEPTION(m_pd3dDevice->CreateTexture2D(&depthStencilDesc, nullptr, texture));
	return true;
}

bool CoreD3D::CreateDepthStencilView(ID3D11Texture2D* texture, ID3D11DepthStencilView** view)
{
	HR_EXCEPTION(m_pd3dDevice->CreateDepthStencilView(texture, nullptr, view));
	return true;
}

bool CoreD3D::CreateD3DTexture(ID3D11Texture2D** texture, bool bRenderTarget, bool gdi, int width, int height, bool bRead)
{
	D3D11_TEXTURE2D_DESC td;
	memset(&td, 0, sizeof(td));
	td.Width = width;
	td.Height = height;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	td.SampleDesc.Count = 1;
	td.CPUAccessFlags = 0;
	
	if (bRenderTarget)
	{
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags |= D3D11_BIND_RENDER_TARGET;
		td.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
		if (gdi)
			td.MiscFlags |= D3D11_RESOURCE_MISC_GDI_COMPATIBLE;
	}
	else
	{
		if (bRead)
		{
			td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			td.Usage = D3D11_USAGE_STAGING;
		}
		else
		{
			td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			td.Usage = D3D11_USAGE_DYNAMIC;
			td.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
		}
		td.MiscFlags = 0;
	}

	HR(m_pd3dDevice->CreateTexture2D(&td, NULL, texture));
	return true;
}

bool CoreD3D::CreateShaderResourceView(ID3D11Texture2D* texture, ID3D11ShaderResourceView** resourceView)
{
	D3D11_SHADER_RESOURCE_VIEW_DESC view_desc_;
	memset(&view_desc_, 0, sizeof(view_desc_));
	view_desc_.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	view_desc_.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	view_desc_.Texture2D.MipLevels = 1;
	HR(m_pd3dDevice->CreateShaderResourceView(texture, &view_desc_, resourceView));
	return true;
}

bool CoreD3D::CreateRenderTargetView(ID3D11Texture2D* texture, ID3D11RenderTargetView** renderView)
{
	HR(m_pd3dDevice->CreateRenderTargetView(texture, nullptr, renderView));
	return true;
}

void CoreD3D::OpenAlphaBlend()
{
	float blendFactor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_pd3dImmediateContext->OMSetBlendState(alpha_blend_state_.Get(), blendFactor, 0xffffffff);
}

void CoreD3D::CloseAlphaBlend()
{
	float blendFactor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_pd3dImmediateContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
}

void CoreD3D::PushFrontViewPort(float width, float height)
{
	D3D11_VIEWPORT viewport;
	memset(&viewport, 0, sizeof(D3D11_VIEWPORT));
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = static_cast<float>(width);
	viewport.Height = static_cast<float>(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport_list_.push_front(viewport);
	m_pd3dImmediateContext->RSSetViewports(1, &viewport);
}

void CoreD3D::GetFrontViewPort(D3D11_VIEWPORT* port)
{
	if (viewport_list_.size())
	{
		D3D11_VIEWPORT& view = viewport_list_.front();
		memcpy(port, &view, sizeof(D3D11_VIEWPORT));
	}
}

void CoreD3D::PopFrontViewPort()
{
	viewport_list_.pop_front();
	if (viewport_list_.size())
	{
		D3D11_VIEWPORT& view = viewport_list_.front();
		m_pd3dImmediateContext->RSSetViewports(1, &view);
	}
}

void CoreD3D::PSSetShaderResources(int startslots, int numviews, ID3D11ShaderResourceView** view)
{
	m_pd3dImmediateContext->PSSetShaderResources(startslots, numviews, view);
}

void CoreD3D::DrawIndexed(UINT indexcount, UINT startlocation, UINT baselocation)
{
	m_pd3dImmediateContext->DrawIndexed(indexcount, startlocation, baselocation);
}

void CoreD3D::Map(ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource)
{
	m_pd3dImmediateContext->Map(pResource, Subresource, MapType, MapFlags, pMappedResource);
}

void CoreD3D::UnMap(ID3D11Resource* pResource, UINT Subresource)
{
	m_pd3dImmediateContext->Unmap(pResource, Subresource);
}

void CoreD3D::CopyResource(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource)
{
	m_pd3dImmediateContext->CopyResource(pDstResource, pSrcResource);
}

void CoreD3D::OMGetRenderTargets(UINT NumViews, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView)
{
	m_pd3dImmediateContext->OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView);
}

void CoreD3D::OMSetRenderTargets(UINT NumViews, ID3D11RenderTargetView* const* ppRenderTargetViews, ID3D11DepthStencilView* pDepthStencilView)
{
	m_pd3dImmediateContext->OMSetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView);
}

void CoreD3D::ClearRenderTargetView(ID3D11RenderTargetView* pRenderTargetView, const FLOAT ColorRGBA[4])
{
	m_pd3dImmediateContext->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
}

void CoreD3D::ClearDepthStencilView(ID3D11DepthStencilView* pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)
{
	m_pd3dImmediateContext->ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
}
