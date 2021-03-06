#ifndef CORE_FILTER_H
#define CORE_FILTER_H

#include "core-component-i.h"
#include <map>
#include <string>
#include <base-filter-i.h>
#include <mutex>
#include <list>
#include <functional>

class IBaseSource;

using FuncSourceRender = std::function<void()>;
using FuncSourceRect = std::function<void(float&, float&, float&, float&)>;

class CoreFilter : public ICoreComponent
{
public:
	CoreFilter();
	virtual ~CoreFilter();
	void AppendFilter(std::string name, IBaseFilter* filter);
	IBaseFilter* GetNamedFilter(std::string name);
	void TextureFilters(IBaseSource* source);
	size_t GetFiltersSize();

private:
	void ResetFilterResource(size_t sourceWidth, size_t sourceHeight);
	void RenderSourcesWidthFilters(IBaseSource* source);
	void RenderSourcesOnly(IBaseSource* source);

private:
	std::mutex filter_mutex_;
	std::list<IBaseFilter*> filter_list_;

	ComPtr< ID3D11Texture2D> filter_render_texture_;
	ComPtr< ID3D11RenderTargetView> filter_target_view_;
	ComPtr< ID3D11Texture2D> filer_final_render_texture_;
	ComPtr< ID3D11ShaderResourceView> filter_final_render_view_;
	ComPtr<ID3D11Texture2D> filter_depth_stencil_buffer_;
	ComPtr<ID3D11DepthStencilView> filter_depth_stencil_view_;
	int texture_width_ = 0;
	int texture_height_ = 0;
};

#endif