#pragma once

#include <Windows.h>
#include <stdint.h>
#include <string>
#include <stdexcept>

namespace BitmapTexture
{

	class Bitmap
	{
	public:
		inline virtual ~Bitmap() { if (_bits) delete[]_bits; _bits = NULL; }
		inline Bitmap(int width, int height) : _w(width), _h(height) {
			_pitch = width * 4;
			_bits = new uint8_t[_pitch * _h];
			Fill(0);
		}

		inline Bitmap(const Bitmap& src) : _w(src._w), _h(src._h), _pitch(src._pitch) {
			_bits = new uint8_t[_pitch * _h];
			memcpy(_bits, src._bits, _pitch * _h);
		}

		inline Bitmap(const char* filename) {
			Bitmap* tmp = LoadFile(filename);
			if (tmp == NULL) {
				std::string msg = "load failed: ";
				msg.append(filename);
				throw std::runtime_error(msg);
			}
			_w = tmp->_w; _h = tmp->_h; _pitch = tmp->_pitch; _bits = tmp->_bits;
			tmp->_bits = NULL;
			delete tmp;
		}

	public:
		inline int GetW() const { return _w; }
		inline int GetH() const { return _h; }
		inline int GetPitch() const { return _pitch; }
		inline uint8_t* GetBits() { return _bits; }
		inline const uint8_t* GetBits() const { return _bits; }
		inline uint8_t* GetLine(int y) { return _bits + _pitch * y; }
		inline const uint8_t* GetLine(int y) const { return _bits + _pitch * y; }

	public:

		inline void Fill(uint32_t color) {
			for (int j = 0; j < _h; j++) {
				uint32_t* row = (uint32_t*)(_bits + j * _pitch);
				for (int i = 0; i < _w; i++, row++)
					memcpy(row, &color, sizeof(uint32_t));
			}
		}

		inline void SetPixel(int x, int y, uint32_t color) {
			if (x >= 0 && x < _w && y >= 0 && y < _h) {
				memcpy(_bits + y * _pitch + x * 4, &color, sizeof(uint32_t));
			}
		}

		inline uint32_t GetPixel(int x, int y) const {
			uint32_t color = 0;
			if (x >= 0 && x < _w && y >= 0 && y < _h) {
				memcpy(&color, _bits + y * _pitch + x * 4, sizeof(uint32_t));
			}
			return color;
		}

		inline void DrawLine(int x1, int y1, int x2, int y2, uint32_t color) {
			int x, y;
			if (x1 == x2 && y1 == y2) {
				SetPixel(x1, y1, color);
				return;
			}
			else if (x1 == x2) {
				int inc = (y1 <= y2) ? 1 : -1;
				for (y = y1; y != y2; y += inc) SetPixel(x1, y, color);
				SetPixel(x2, y2, color);
			}
			else if (y1 == y2) {
				int inc = (x1 <= x2) ? 1 : -1;
				for (x = x1; x != x2; x += inc) SetPixel(x, y1, color);
				SetPixel(x2, y2, color);
			}
			else {
				int dx = (x1 < x2) ? x2 - x1 : x1 - x2;
				int dy = (y1 < y2) ? y2 - y1 : y1 - y2;
				int rem = 0;
				if (dx >= dy) {
					if (x2 < x1) x = x1, y = y1, x1 = x2, y1 = y2, x2 = x, y2 = y;
					for (x = x1, y = y1; x <= x2; x++) {
						SetPixel(x, y, color);
						rem += dy;
						if (rem >= dx) { rem -= dx; y += (y2 >= y1) ? 1 : -1; SetPixel(x, y, color); }
					}
					SetPixel(x2, y2, color);
				}
				else {
					if (y2 < y1) x = x1, y = y1, x1 = x2, y1 = y2, x2 = x, y2 = y;
					for (x = x1, y = y1; y <= y2; y++) {
						SetPixel(x, y, color);
						rem += dx;
						if (rem >= dy) { rem -= dy; x += (x2 >= x1) ? 1 : -1; SetPixel(x, y, color); }
					}
					SetPixel(x2, y2, color);
				}
			}
		}

		struct BITMAPINFOHEADER { // bmih  
			uint32_t	biSize;
			uint32_t	biWidth;
			int32_t		biHeight;
			uint16_t	biPlanes;
			uint16_t	biBitCount;
			uint32_t	biCompression;
			uint32_t	biSizeImage;
			uint32_t	biXPelsPerMeter;
			uint32_t	biYPelsPerMeter;
			uint32_t	biClrUsed;
			uint32_t	biClrImportant;
		};

		// 读取 BMP 图片，支持 24/32 位两种格式
		inline static Bitmap* LoadFile(const char* filename) {
			FILE* fp = fopen(filename, "rb");
			if (fp == NULL) return NULL;
			BITMAPINFOHEADER info;
			uint8_t header[14];
			int hr = (int)fread(header, 1, 14, fp);
			if (hr != 14) { fclose(fp); return NULL; }
			if (header[0] != 0x42 || header[1] != 0x4d) { fclose(fp); return NULL; }
			hr = (int)fread(&info, 1, sizeof(info), fp);
			if (hr != 40) { fclose(fp); return NULL; }
			if (info.biBitCount != 24 && info.biBitCount != 32) { fclose(fp); return NULL; }
			Bitmap* bmp = new Bitmap(info.biWidth, info.biHeight);
			uint32_t offset;
			memcpy(&offset, header + 10, sizeof(uint32_t));
			fseek(fp, offset, SEEK_SET);
			uint32_t pixelsize = (info.biBitCount + 7) / 8;
			uint32_t pitch = (pixelsize * info.biWidth + 3) & (~3);
			for (int y = 0; y < (int)info.biHeight; y++) {
				uint8_t* line = bmp->GetLine(info.biHeight - 1 - y);
				for (int x = 0; x < (int)info.biWidth; x++, line += 4) {
					line[3] = 255;
					fread(line, pixelsize, 1, fp);
				}
				fseek(fp, pitch - info.biWidth * pixelsize, SEEK_CUR);
			}
			fclose(fp);
			return bmp;
		}

		// 保存 BMP 图片
		inline bool SaveFile(const char* filename, bool withAlpha = false) const {
			FILE* fp = fopen(filename, "wb");
			if (fp == NULL) return false;
			BITMAPINFOHEADER info;
			uint32_t pixelsize = (withAlpha) ? 4 : 3;
			uint32_t pitch = (GetW() * pixelsize + 3) & (~3);
			info.biSizeImage = pitch * GetH();
			uint32_t bfSize = 54 + info.biSizeImage;
			uint32_t zero = 0, offset = 54;
			fputc(0x42, fp);
			fputc(0x4d, fp);
			fwrite(&bfSize, 4, 1, fp);
			fwrite(&zero, 4, 1, fp);
			fwrite(&offset, 4, 1, fp);
			info.biSize = 40;
			info.biWidth = GetW();
			info.biHeight = GetH();
			info.biPlanes = 1;
			info.biBitCount = (withAlpha) ? 32 : 24;
			info.biCompression = 0;
			info.biXPelsPerMeter = 0xb12;
			info.biYPelsPerMeter = 0xb12;
			info.biClrUsed = 0;
			info.biClrImportant = 0;
			fwrite(&info, sizeof(info), 1, fp);
			// printf("pitch=%d %d\n", (int)pitch, info.biSizeImage);
			for (int y = 0; y < GetH(); y++) {
				const uint8_t* line = GetLine(info.biHeight - 1 - y);
				uint32_t padding = pitch - GetW() * pixelsize;
				for (int x = 0; x < GetW(); x++, line += 4) {
					fwrite(line, pixelsize, 1, fp);
				}
				for (int i = 0; i < (int)padding; i++) fputc(0, fp);
			}
			fclose(fp);
			return true;
		}

		// 上下反转
		inline void FlipVertical() {
			uint8_t* buffer = new uint8_t[_pitch];
			for (int i = 0, j = _h - 1; i < j; i++, j--) {
				memcpy(buffer, GetLine(i), _pitch);
				memcpy(GetLine(i), GetLine(j), _pitch);
				memcpy(GetLine(j), buffer, _pitch);
			}
			delete[]buffer;
		}

		// 水平反转
		inline void FlipHorizontal() {
			for (int y = 0; y < _h; y++) {
				for (int i = 0, j = _w - 1; i < j; i++, j--) {
					uint32_t c1 = GetPixel(i, y);
					uint32_t c2 = GetPixel(j, y);
					SetPixel(i, y, c2);
					SetPixel(j, y, c1);
				}
			}
		}

	protected:
		int32_t _w;
		int32_t _h;
		int32_t _pitch;
		uint8_t* _bits;
	};
}