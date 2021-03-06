#ifndef GDIPLUS_TEXT_SOURCE_H
#define GDIPLUS_TEXT_SOURCE_H
#include "base-source-i.h"
#include <atomic>
#include <gdiplus.h>
#include <memory>

enum class Align {
	Left,
	Center,
	Right,
};

enum class VAlign {
	Top,
	Center,
	Bottom,
};

template<typename T, typename T2, BOOL WINAPI deleter(T2)> class GDIObj {
	T obj = nullptr;

	inline GDIObj& Replace(T obj_)
	{
		if (obj)
			deleter(obj);
		obj = obj_;
		return *this;
	}

public:
	inline GDIObj() {}
	inline GDIObj(T obj_) : obj(obj_) {}
	inline ~GDIObj() { deleter(obj); }

	inline T operator=(T obj_)
	{
		Replace(obj_);
		return obj;
	}

	inline operator T() const { return obj; }

	inline bool operator==(T obj_) const { return obj == obj_; }
	inline bool operator!=(T obj_) const { return obj != obj_; }
};

using HDCObj = GDIObj<HDC, HDC, DeleteDC>;
using HFONTObj = GDIObj<HFONT, HGDIOBJ, DeleteObject>;
using HBITMAPObj = GDIObj<HBITMAP, HGDIOBJ, DeleteObject>;

class GdiplusTextSource : public IBaseSource
{
public:
	GdiplusTextSource();
	virtual ~GdiplusTextSource();

	static void ModuleLoad();
	static void ModuleUnLoad();

	virtual bool Init();

	virtual bool Render();

protected:
	virtual bool Update(const char* json);
	virtual bool Tick();
	virtual void UpdateTextureSize();

private:
	void UpdateFont();
	void RenderText();
	void GetStringFormat(Gdiplus::StringFormat& format);
	void CalculateTextSizes(const Gdiplus::StringFormat& format,
		Gdiplus::RectF& bounding_box, SIZE& text_size);
	void RemoveNewlinePadding(const Gdiplus::StringFormat& format, Gdiplus::RectF& box);

private:
	ComPtr< ID3D11Texture2D> m_pTexture;
	ComPtr<ID3D11ShaderResourceView> m_pResourceView;
	ComPtr<ID3D11BlendState> BSTransparent = nullptr;
	std::vector<uint8_t> text_data_;
	std::vector<D3D11_SUBRESOURCE_DATA> srd;
	std::atomic<bool> text_update_;

	HDCObj hdc_;
	Gdiplus::Graphics graphics_;
	HFONTObj hfont;
	std::unique_ptr<Gdiplus::Font> font;
	int face_size = 25;
	bool bold = true;
	bool italic = false;
	bool underline = false;
	bool strikeout = false;
	bool vertical = false;
	Align align = Align::Left;
	VAlign valign = VAlign::Top;
	std::wstring text = L"";
	int width = 0;
	int height = 0;
};


#endif