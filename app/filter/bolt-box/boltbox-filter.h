#ifndef BOLTBOX_FILTER_H
#define BOLTBOX_FILTER_H

#include <vector>
#include <string>
#include "base-filter-i.h"
#include "bitmap-texture-file.h"

class BoltBoxFilter : public IBaseFilter
{
	enum BoltDirection : int
	{
		kStart = 0,
		kUp,
		kRight,
		kDown,
		kLeft,
		kEnd
	};

public:
	BoltBoxFilter();
	virtual ~BoltBoxFilter();

	virtual bool Init();
	virtual void SetInputTexture(ID3D11Texture2D* texture, size_t width, size_t height, uint8_t* data = nullptr);
	virtual void CopyOutputTexture(ID3D11Texture2D* texture);
	virtual void Update(const char* jsondata);

private:
	void ResetFilterResource(int width, int height);
	void InitBoltBitmapRes(const char* path);
	void ResetBoltResource(int width, int height);
	void LoadBoltResource();
	UINT LoadBoltVertexBuffer();

private:
	int texture_width_ = 0;
	int texture_height_ = 0;
	int bolt_lineheight_ = 50;
	int cur_bolt_frame_ = 0;
	int max_bolt_frame_ = 60;
	int cur_step_cnt_ = 0;
	ComPtr< ID3D11Texture2D> output_texture_;
	ComPtr<ID3D11ShaderResourceView> output_texture_view_;
	ComPtr< ID3D11Texture2D> render_texture_;
	ComPtr< ID3D11RenderTargetView> render_target_view_;
	ComPtr< ID3D11Texture2D> bolt_texture_;
	ComPtr< ID3D11ShaderResourceView > bolt_shader_resource_;
	ComPtr<ID3D11Buffer> vertex_buffer_;
	ComPtr<ID3D11Buffer> index_buffer_;
	ComPtr<ID3D11Texture2D> depth_stencil_buffer_;
	ComPtr<ID3D11DepthStencilView> depth_stencil_view_;

	std::vector< BitmapTexture::Bitmap*> bolt_bitmap_vec_;

	BoltDirection bolt_direction_ = BoltDirection::kStart;
};



#endif