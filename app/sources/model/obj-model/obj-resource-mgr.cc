#include "obj-resource-mgr.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

using namespace BitmapTexture;

ObjResourceMgr::ObjResourceMgr()
{
	
}

ObjResourceMgr::~ObjResourceMgr()
{
	if (diffuse_bitmap_)
	{
		delete diffuse_bitmap_;
		diffuse_bitmap_ = nullptr;
	}
	if (normal_mapping_bitmap_)
	{
		delete normal_mapping_bitmap_;
		normal_mapping_bitmap_ = nullptr;
	}
	if (specular_bitmap_)
	{
		delete specular_bitmap_;
		specular_bitmap_ = nullptr;
	}
}

bool ObjResourceMgr::Init(const char* modelpath)
{
	/*
	* obj 模型文件夹下，查找.obj模型文件， *_diffuse.bmp 漫反射纹理图片, *_nm.bmp 法线贴图纹理图片, *_spec.bmp 镜面光照纹理图片
	*/
	try
	{
		for (auto& DirectoryIter : std::filesystem::directory_iterator(modelpath))
		{
			auto filepath = DirectoryIter.path();
			auto filename = filepath.filename();

			if (filename.string().find(".obj") != std::string::npos)
			{
				ParseObjFile(filepath.string().c_str());
			}
			else if (filename.string().find("diffuse.bmp") != std::string::npos)
			{
				ParseTexturePicFile(filepath.string().c_str(), "_diffuse.bmp", &diffuse_bitmap_);
			}
			else if (filename.string().find("nm.bmp") != std::string::npos)
			{
				ParseTexturePicFile(filepath.string().c_str(), "_nm.bmp", &normal_mapping_bitmap_);
			}
			else if (filename.string().find("spec.bmp") != std::string::npos)
			{
				ParseTexturePicFile(filepath.string().c_str(), "_spec.bmp", &specular_bitmap_);
			}
		}

		if (!verts_.empty() && !nmmap_.empty() && !texcooduv_.empty() && !face_vertex_.empty() && !face_index_.empty() &&
			diffuse_bitmap_ && normal_mapping_bitmap_ && specular_bitmap_)
			return true;

		return false;
	}
	catch (...)
	{

	}

	return false;
}

bool ObjResourceMgr::ParseObjFile(const char* filename)
{
	std::ifstream in;
	in.open(filename, std::ifstream::in);
	if (in.fail()) return false;
	std::string line;
	while (!in.eof()) {
		std::getline(in, line);
		std::istringstream iss(line.c_str());
		char trash;
		if (line.compare(0, 2, "v ") == 0) {
			iss >> trash;
			DirectX::XMFLOAT3 pos;
			iss >> pos.x;
			iss >> pos.y;
			iss >> pos.z;
			//pos.z = -pos.z;
			verts_.push_back(pos);
		}
		else if (line.compare(0, 3, "vn ") == 0) {
			iss >> trash >> trash;
			DirectX::XMFLOAT3 cood;
			iss >> cood.x;
			iss >> cood.y;
			iss >> cood.z;
			//cood.z = -cood.z;
			nmmap_.push_back(cood);
		}
		else if (line.compare(0, 3, "vt ") == 0) {
			iss >> trash >> trash;
			DirectX::XMFLOAT2 uv;
			iss >> uv.x;
			iss >> uv.y;
			uv.y = -uv.y;
			texcooduv_.push_back(uv);
		}
		else if (line.compare(0, 2, "f ") == 0) {
			VertexPosNormalTex vertex;
			DWORD vpi[3], vni[3], vti[3];
			iss >> trash;
			for (int i = 0; i < 3; i++)
			{
				iss >> vpi[i];
				iss >> trash;
				iss >> vti[i];
				iss >> trash;
				iss >> vni[i];
			}

			for (int i = 0; i < 3; ++i)
			{
				vertex.pos = verts_[vpi[i] - 1];
				vertex.tex = texcooduv_[vti[i] - 1];
				vertex.normal = nmmap_[vni[i] - 1];
				AddVertex(vertex, vpi[i], vti[i], vni[i]);
			}
		}
	}

	return true;
}

bool ObjResourceMgr::ParseTexturePicFile(const char* filename, const char* suffix, Bitmap** bitmap)
{
	*bitmap = new Bitmap(filename);
	if(bitmap)
		return true;
	return false;
}

void ObjResourceMgr::AddVertex(const VertexPosNormalTex& vertex, float vpi, float vti, float vni)
{
	std::wstring idxStr = std::to_wstring(vpi) + L"/" + std::to_wstring(vti) + L"/" + std::to_wstring(vni);

	// 寻找是否有重复顶点
	auto it = vertexCache.find(idxStr);
	if (it != vertexCache.end())
	{
		face_index_.push_back(it->second);
	}
	else
	{
		face_vertex_.push_back(vertex);
		DWORD pos = (DWORD)face_vertex_.size() - 1;
		vertexCache[idxStr] = pos;
		face_index_.push_back(pos);
	}
}