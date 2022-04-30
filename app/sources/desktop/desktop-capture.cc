#include "desktop-capture.h"
#include "logger.h"
#include "core-engine.h"
#include "core-d3d.h"

DesktopCaptureSource::DesktopCaptureSource()
{

}

DesktopCaptureSource::~DesktopCaptureSource()
{

}

bool DesktopCaptureSource::Init()
{
	CoreD3D* d3d = core_engine_->GetD3D();
	int monitor_width = 0, monitor_height = 0;
	HDC hdcScreen;
	hdcScreen = CreateDC(L"DISPLAY", NULL, NULL, NULL);

	monitor_width = GetDeviceCaps(hdcScreen, HORZRES);    // pixel
	monitor_height = GetDeviceCaps(hdcScreen, VERTRES);    // pixel
	texture_width_ = monitor_width;
	texture_height_ = monitor_height;

	d3d->CreateD3DTexture(m_pTexture.GetAddressOf(), true, true, monitor_width, monitor_height, false);
	d3d->CreateShaderResourceView(m_pTexture.Get(), m_pResourceView.GetAddressOf());

	ComPtr<IDXGIOutput1> output1;
	ComPtr<IDXGIOutput> output;
	HR(d3d->GetD3DAdapter().Get()->EnumOutputs(0, output.GetAddressOf()));
	HR(output->QueryInterface(__uuidof(IDXGIOutput1), (void**)output1.GetAddressOf()));
	HR(output1->DuplicateOutput(d3d->GetD3DDevice().Get(), m_pDuplication.GetAddressOf()));

	return true;
}

bool DesktopCaptureSource::Update(const char* json)
{
	return true;
}

void DesktopCaptureSource::UpdateTextureSize()
{
	source_texture_size_.width = texture_width_;
	source_texture_size_.height = texture_height_;
}

bool DesktopCaptureSource::Tick()
{
	CoreD3D* d3d = core_engine_->GetD3D();
	DXGI_OUTDUPL_FRAME_INFO info;
	ComPtr<IDXGIResource> res;
	ComPtr<ID3D11Texture2D> tex;
	HRESULT hr = m_pDuplication->AcquireNextFrame(0, &info, res.GetAddressOf());
	if (hr == DXGI_ERROR_ACCESS_LOST) {
		return false;
	}
	else if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
		return false;
	}
	else if (FAILED(hr)) {
		return false;
	}
	HR(res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)tex.GetAddressOf()));
	d3d->CopyResource(m_pTexture.Get(), tex.Get());
	m_pDuplication->ReleaseFrame();
	return true;
}

bool DesktopCaptureSource::Render()
{
	CoreD3D* d3d = core_engine_->GetD3D();
	d3d->UpdateVertexShader(CoreD3DData::VertexHlslType::kBasic2D);
	d3d->UpdatePixelShader(CoreD3DData::PixelHlslType::kBasic2D);
	d3d->PSSetShaderResources(0, 1, m_pResourceView.GetAddressOf());
	Draw2DSource();
	return true;
}
