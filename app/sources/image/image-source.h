#pragma once

#include "base-source-i.h"
#include "dx-header.h"

class ImageSource : public IBaseSource
{
public:
	ImageSource();
	virtual ~ImageSource();

	virtual bool Init();

	virtual bool Render();

protected:
	virtual bool Update(const char* json);
	virtual bool Tick();
	virtual void UpdateTextureSize();

private:
	ComPtr< ID3D11Texture2D> m_pTexture;
	ComPtr<ID3D11ShaderResourceView> m_pResourceView;
	std::string image_file_path_;
	int texture_width_ = 0;
	int texture_height_ = 0;
};