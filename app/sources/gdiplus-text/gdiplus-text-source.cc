#include "gdiplus-text-source.h"
#include <Windows.h>
#include <gdiplus.h>
#include <gdiplusinit.h>
#include <locale>
#include "json.hpp"
#include "core-engine.h"
#include "core-d3d.h"
#include "core-settings.h"

static ULONG_PTR gdip_token_ = 0;

#ifndef clamp
#define clamp(val, min_val, max_val) \
	if (val < min_val)           \
		val = min_val;       \
	else if (val > max_val)      \
		val = max_val;
#endif

#define MIN_SIZE_CX 2
#define MIN_SIZE_CY 2
#define MAX_SIZE_CX 16384
#define MAX_SIZE_CY 16384
#define EPSILON 1e-4f
#define MAX_AREA (4096LL * 4096LL)

std::wstring toWideString(const char* pStr, int len, UINT CodePage /*=CP_ACP*/)
{
	std::wstring buf;

	if (pStr == NULL)
	{
		assert(NULL);
		return buf;
	}

	if (len < 0 && len != -1)
	{
		assert(NULL);
		return buf;
	}

	// figure out how many wide characters we are going to get 
	int nChars = MultiByteToWideChar(CodePage, 0, pStr, len, NULL, 0);
	if (len == -1)
		--nChars;
	if (nChars == 0)
		return L"";

	// convert the narrow string to a wide string 
	// nb: slightly naughty to write directly into the string like this
	buf.resize(nChars);
	MultiByteToWideChar(CodePage, 0, pStr, len,
		const_cast<wchar_t*>(buf.c_str()), nChars);

	return buf;
}

static inline DWORD get_alpha_val(uint32_t opacity)
{
	return ((opacity * 255 / 100) & 0xFF) << 24;
}

static inline DWORD calc_color(uint32_t color, uint32_t opacity)
{
	return color & 0xFFFFFF | get_alpha_val(opacity);
}

void GdiplusTextSource::ModuleLoad()
{
	const  Gdiplus::GdiplusStartupInput gdip_input;
	Gdiplus::GdiplusStartup(&gdip_token_, &gdip_input, nullptr);
}

void GdiplusTextSource::ModuleUnLoad()
{
	Gdiplus::GdiplusShutdown(gdip_token_);
}

GdiplusTextSource::GdiplusTextSource() : hdc_(CreateCompatibleDC(nullptr)),
					graphics_(hdc_)
{
	text_update_.store(false);
}

GdiplusTextSource::~GdiplusTextSource()
{
	if(font)
		font.reset(nullptr);
}

bool GdiplusTextSource::Init()
{
	return true;
}

bool GdiplusTextSource::Update(const char* jsondata)
{
	try
	{
		auto json = nlohmann::json::parse(jsondata);
		if (!json.is_object())
			return false;
		if (json.find("text") != json.end())
		{
			std::string sText = json.at("text");
			std::wstring wText = L"";
			wText = toWideString(sText.c_str(), sText.length(), CP_UTF8);
			wText.push_back('\n');
			if (wText == text)
				return true;
			text = wText;
			text_update_.store(true);
		}
		return true;
	}
	catch (...)
	{

	}
	return false;
}

void GdiplusTextSource::UpdateFont()
{
	hfont = nullptr;
	font.reset(nullptr);

	LOGFONT lf = {};
	lf.lfHeight = face_size;
	lf.lfWeight = bold ? FW_BOLD : FW_DONTCARE;
	lf.lfItalic = italic;
	lf.lfUnderline = underline;
	lf.lfStrikeOut = strikeout;
	lf.lfQuality = ANTIALIASED_QUALITY;
	lf.lfCharSet = DEFAULT_CHARSET;

	if (!hfont) {
		wcscpy(lf.lfFaceName, L"Arial");
		hfont = CreateFontIndirect(&lf);
	}

	if (hfont)
	{
		font.reset(new Gdiplus::Font(hdc_, hfont));
		Gdiplus::FontFamily family;
		font->GetFamily(&family);
		std::wstring fontName;
		family.GetFamilyName(&fontName[0]);
	}
}


void GdiplusTextSource::GetStringFormat(Gdiplus::StringFormat& format)
{
	UINT flags = Gdiplus::StringFormatFlagsNoFitBlackBox |
		Gdiplus::StringFormatFlagsMeasureTrailingSpaces;

	if (vertical)
		flags |= Gdiplus::StringFormatFlagsDirectionVertical |
		Gdiplus::StringFormatFlagsDirectionRightToLeft;

	format.SetFormatFlags(flags);
	format.SetTrimming(Gdiplus::StringTrimmingWord);

	switch (align) {
	case Align::Left:
		if (vertical)
			format.SetLineAlignment(Gdiplus::StringAlignmentFar);
		else
			format.SetAlignment(Gdiplus::StringAlignmentNear);
		break;
	case Align::Center:
		if (vertical)
			format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
		else
			format.SetAlignment(Gdiplus::StringAlignmentCenter);
		break;
	case Align::Right:
		if (vertical)
			format.SetLineAlignment(Gdiplus::StringAlignmentNear);
		else
			format.SetAlignment(Gdiplus::StringAlignmentFar);
	}

	switch (valign) {
	case VAlign::Top:
		if (vertical)
			format.SetAlignment(Gdiplus::StringAlignmentNear);
		else
			format.SetLineAlignment(Gdiplus::StringAlignmentNear);
		break;
	case VAlign::Center:
		if (vertical)
			format.SetAlignment(Gdiplus::StringAlignmentCenter);
		else
			format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
		break;
	case VAlign::Bottom:
		if (vertical)
			format.SetAlignment(Gdiplus::StringAlignmentFar);
		else
			format.SetLineAlignment(Gdiplus::StringAlignmentFar);
	}
}

void GdiplusTextSource::RemoveNewlinePadding(const Gdiplus::StringFormat& format, Gdiplus::RectF& box)
{
	Gdiplus::RectF before;
	Gdiplus::RectF after;
	Gdiplus::Status stat;

	stat = graphics_.MeasureString(L"W", 2, font.get(), Gdiplus::PointF(0.0f, 0.0f),
		&format, &before);

	stat = graphics_.MeasureString(L"W\n", 3, font.get(), Gdiplus::PointF(0.0f, 0.0f),
		&format, &after);

	float offset_cx = after.Width - before.Width;
	float offset_cy = after.Height - before.Height;

	if (!vertical) {
		if (offset_cx >= 1.0f)
			offset_cx -= 1.0f;

		if (valign == VAlign::Center)
			box.Y -= offset_cy * 0.5f;
		else if (valign == VAlign::Bottom)
			box.Y -= offset_cy;
	}
	else {
		if (offset_cy >= 1.0f)
			offset_cy -= 1.0f;

		if (align == Align::Center)
			box.X -= offset_cx * 0.5f;
		else if (align == Align::Right)
			box.X -= offset_cx;
	}

	box.Width -= offset_cx;
	box.Height -= offset_cy;
}

void GdiplusTextSource::CalculateTextSizes(const Gdiplus::StringFormat& format,
	Gdiplus::RectF& bounding_box, SIZE& text_size)
{
	Gdiplus::RectF layout_box;
	Gdiplus::RectF temp_box;
	Gdiplus::Status stat;

	stat = graphics_.MeasureString(
		text.c_str(), (int)text.size() + 1, font.get(),
		Gdiplus::PointF(0.0f, 0.0f), &format, &bounding_box);
	temp_box = bounding_box;
	bounding_box.X = 0.0f;
	bounding_box.Y = 0.0f;

	RemoveNewlinePadding(format, bounding_box);

	if (vertical) {
		if (bounding_box.Width < face_size) {
			text_size.cx = face_size;
			bounding_box.Width = float(face_size);
		}
		else {
			text_size.cx = LONG(bounding_box.Width + EPSILON);
		}

		text_size.cy = LONG(bounding_box.Height + EPSILON);
	}
	else {
		if (bounding_box.Height < face_size) {
			text_size.cy = face_size;
			bounding_box.Height = float(face_size);
		}
		else {
			text_size.cy = LONG(bounding_box.Height + EPSILON);
		}

		text_size.cx = LONG(bounding_box.Width + EPSILON);
	}

	text_size.cx += text_size.cx % 2;
	text_size.cy += text_size.cy % 2;

	int64_t total_size = int64_t(text_size.cx) * int64_t(text_size.cy);

	/* GPUs typically have texture size limitations */
	clamp(text_size.cx, MIN_SIZE_CX, MAX_SIZE_CX);
	clamp(text_size.cy, MIN_SIZE_CY, MAX_SIZE_CY);

	/* avoid taking up too much VRAM */
	if (total_size > MAX_AREA) {
		if (text_size.cx > text_size.cy)
			text_size.cx = (LONG)MAX_AREA / text_size.cy;
		else
			text_size.cy = (LONG)MAX_AREA / text_size.cx;
	}

	/* the internal text-rendering bounding box for is reset to
	 * its internal value in case the texture gets cut off */
	bounding_box.Width = temp_box.Width;
	bounding_box.Height = temp_box.Height;
}


void GdiplusTextSource::RenderText()
{
	CoreD3D* d3d = core_engine_->GetD3D();
	m_pTexture.Reset();
	m_pResourceView.Reset();

	Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());
	Gdiplus::Status stat;

	Gdiplus::RectF box;
	SIZE size;

	GetStringFormat(format);
	CalculateTextSizes(format, box, size);

	std::unique_ptr<uint8_t[]> bits(new uint8_t[size.cx * size.cy * 4]);
	Gdiplus::Bitmap bitmap(size.cx, size.cy, 4 * size.cx, PixelFormat32bppARGB, bits.get());

	Gdiplus::Graphics graphics_bitmap(&bitmap);
	Gdiplus::LinearGradientBrush brush(Gdiplus::RectF(0, 0, (float)size.cx, (float)size.cy),
		Gdiplus::Color(calc_color(0xFFFFFF, 100)),
		Gdiplus::Color(calc_color(0xFFFFFF, 100)),
		0, 1);

	DWORD full_bk_color = 0;

	if (!text.empty())
		full_bk_color |= get_alpha_val(0);

	if (size.cx > box.Width || size.cy > box.Height) {
		stat = graphics_bitmap.Clear(Gdiplus::Color(0));
		Gdiplus::SolidBrush bk_brush = Gdiplus::Color(full_bk_color);
		stat = graphics_bitmap.FillRectangle(&bk_brush, box);
	}
	else {
		stat = graphics_bitmap.Clear(Gdiplus::Color(full_bk_color));
	}
	graphics_bitmap.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
	graphics_bitmap.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
	graphics_bitmap.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

	stat = graphics_bitmap.DrawString(text.c_str(),
		(int)text.size(),
		font.get(), box,
		&format, &brush);
	width = size.cx;
	height = size.cy;

	d3d->CreateD3DTexture(m_pTexture.GetAddressOf(), false, false, width, height, false);
	d3d->CreateShaderResourceView(m_pTexture.Get(), m_pResourceView.GetAddressOf());

	uint8_t* ptr;
	uint32_t linesize_out;
	size_t row_copy;
	size_t height;
	uint8_t* data = bits.get();

	D3D11_MAPPED_SUBRESOURCE map;

	d3d->Map(m_pTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);

	ptr = (uint8_t*)map.pData;
	linesize_out = map.RowPitch;
	uint32_t linesize = size.cx * 4;
	row_copy = (linesize < linesize_out) ? linesize : linesize_out;
	height = size.cy;
	if (linesize == linesize_out) {
		memcpy(ptr, data, row_copy * height);
	}
	else {
		uint8_t* const end = ptr + height * linesize_out;
		while (ptr < end) {
			memcpy(ptr, data, row_copy);
			ptr += linesize_out;
			data += linesize;
		}
	}

	d3d->UnMap(m_pTexture.Get(), 0);
}

bool GdiplusTextSource::Tick()
{
	if (text_update_.load())
	{
		UpdateFont();
		RenderText();
		text_update_.store(false);
	}
	return true;
}

void GdiplusTextSource::UpdateTextureSize()
{
	source_texture_size_.width = width;
	source_texture_size_.height = height;
}


bool GdiplusTextSource::Render()
{
	if (m_pTexture)
	{
		CoreD3D* d3d = core_engine_->GetD3D();
		d3d->OpenAlphaBlend();
		d3d->UpdateVertexShader(CoreD3DData::VertexHlslType::kBasic2D);
		d3d->UpdatePixelShader(CoreD3DData::PixelHlslType::kBasic2D);
		d3d->PSSetShaderResources(0, 1, m_pResourceView.GetAddressOf());
		Draw2DSource();
		d3d->CloseAlphaBlend();
		return true;
	}
	return false;
}
