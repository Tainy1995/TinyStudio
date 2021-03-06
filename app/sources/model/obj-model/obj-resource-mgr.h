#pragma once

/*
* 仅做obj资源文件和相关文件的解析
*/

#include <string>
#include <bitmap-texture-file.h>
#include <dx-header.h>

class ObjResourceMgr
{
public:
	ObjResourceMgr();
	~ObjResourceMgr();

	bool Init(const char* modelpath);

private:
	bool ParseObjFile(const char* file);
	bool ParseTexturePicFile(const char* file, const char* suffix, BitmapTexture::Bitmap** bitmap);
	void AddVertex(const VertexPosNormalTex& vertex, float vpi, float vti, float vni);

public:
	BitmapTexture::Bitmap* diffuse_bitmap_ = nullptr;
	BitmapTexture::Bitmap* normal_mapping_bitmap_ = nullptr;
	BitmapTexture::Bitmap* specular_bitmap_ = nullptr;

	std::vector< DirectX::XMFLOAT3 > verts_;
	std::vector< DirectX::XMFLOAT3 > nmmap_;
	std::vector< DirectX::XMFLOAT2 > texcooduv_;

	std::vector<DWORD> face_index_;
	std::vector< VertexPosNormalTex> face_vertex_;

	std::unordered_map<std::wstring, DWORD> vertexCache;
};