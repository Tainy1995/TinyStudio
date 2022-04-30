#ifndef MEDIA_SOURCE_LOGIC_H
#define MEDIA_SOURCE_LOGIC_H

#include "base-source-i.h"
#include "media-controler.h"
#include <memory>

class MediaSourceLogic : public IBaseSource
{
public:
	MediaSourceLogic();
	virtual ~MediaSourceLogic();

	virtual bool Init();
	virtual bool Render();

protected:
	virtual bool Update(const char* jsondata);
	virtual bool Tick();
	virtual void UpdateTextureSize();

private:
	void MediaStopped();

private:
	ComPtr< ID3D11Texture2D> m_pTexture;
	ComPtr<ID3D11ShaderResourceView> m_pResourceView;
	std::unique_ptr<MediaControler> media_controler_;
	int texture_width_ = 0;
	int texture_height_ = 0;

};


#endif