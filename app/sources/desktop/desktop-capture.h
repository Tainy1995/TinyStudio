#pragma once

#include "base-source-i.h"
#include "dx-header.h"

class DesktopCaptureSource : public IBaseSource
{
public:
	DesktopCaptureSource();
	virtual ~DesktopCaptureSource();

	virtual bool Init();
	virtual bool Render();

protected:
	virtual bool Update(const char* json);
	virtual bool Tick();
	virtual void UpdateTextureSize();

private:
	ComPtr< IDXGIOutputDuplication> m_pDuplication;
	ComPtr< ID3D11Texture2D> m_pTexture;
	ComPtr<ID3D11ShaderResourceView> m_pResourceView;
	int texture_width_ = 0;
	int texture_height_ = 0;
};