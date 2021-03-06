#ifndef SPLIT_FILTER_H
#define SPLIT_FILTER_H

#include <vector>
#include <string>
#include "base-filter-i.h"

class SplitFilter : public IBaseFilter
{
public:
	SplitFilter();
	virtual ~SplitFilter();

	virtual bool Init();
	virtual void SetInputTexture(ID3D11Texture2D* texture, size_t width, size_t height, uint8_t* data = nullptr);
	virtual void CopyOutputTexture(ID3D11Texture2D* texture);
	virtual void Update(const char* jsondata);

private:
	void ResetFilterResource(int width, int height);

private:
	ComPtr< ID3D11Texture2D> output_texture_;
	ComPtr<ID3D11ShaderResourceView> output_texture_view_;
	ComPtr< ID3D11Texture2D> scale_render_texture_;
	ComPtr< ID3D11RenderTargetView> scale_render_target_;
	int texture_width_ = 0;
	int texture_height_ = 0;

};



#endif