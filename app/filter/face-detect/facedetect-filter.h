#ifndef FACEDETECTFILTER_H
#define FACEDETECTFILTER_H

#include <string>
#include <d2d1.h>
#include "ai-detect-mgr.h"
#include "base-filter-i.h"
#include "dx-header.h"

class FaceDetectFilter : public IBaseFilter
{
public:
	FaceDetectFilter();
	virtual ~FaceDetectFilter();

	virtual bool Init();
	virtual void Update(const char* jsondata);
	virtual void SetInputTexture(ID3D11Texture2D* texture, size_t width, size_t height, uint8_t* data = nullptr);
	virtual void CopyOutputTexture(ID3D11Texture2D* texture);

private:
	bool InitResource(size_t width, size_t height);

private:
	ComPtr<ID2D1Factory> factory_;
	ComPtr<IDXGISurface1> dxgi_surface_;
	ComPtr<ID2D1RenderTarget> paint_render_target_;
	ComPtr<ID2D1Bitmap> render_target_bitmap_;
	ComPtr<ID2D1StrokeStyle> stroke_style_;
	ComPtr<ID2D1SolidColorBrush> rect_brush_;
	ComPtr<ID2D1SolidColorBrush> pen_brush;

	ComPtr< ID3D11Texture2D> face_render_target_;		//渲染合图
	ComPtr< ID3D11RenderTargetView> face_rendertarget_view_;		//渲染合图的view
	ComPtr< ID3D11Texture2D> input_texture_;				//输入的摄像头纹理
	ComPtr<ID3D11ShaderResourceView> input_texture_resource_view_;	//输入纹理的resourceview
	ComPtr< ID3D11Texture2D> input_scale_texture_;				//输入的摄像头纹理
	ComPtr<ID3D11RenderTargetView> input_scale_resource_view_;	//输入纹理的resourceview
	ComPtr< ID3D11Texture2D> input_scale_read_texture_;				//输入的摄像头纹理
	ComPtr< ID3D11Texture2D> paint_texture_;				//画图的纹理
	ComPtr< ID3D11Texture2D> copy_paint_texture_;		//用来读取画图的纹理
	ComPtr<ID3D11ShaderResourceView> copy_paint_texture_resource_view_;	//用来读取画图的纹理的resourceview
	ComPtr<ID3D11VertexShader> vertex_shader_;				// 用于2D的顶点着色器
	ComPtr<ID3D11PixelShader> pixel_shader_;				    // 用于2D的像素着色器
	ComPtr<ID3D11InputLayout> vertex_layout_;

	bool init_result_ = false;
	AiDetectMgr ai_detect_mgr_;

	int scale_width_ = 320;
	int scale_height_ = 0;

	float dpi_x_ = 0.0f;
	float dpi_y_ = 0.0f;
};

#endif