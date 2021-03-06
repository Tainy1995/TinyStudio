#include "obj-render-mgr.h"
#include "core-d3d.h"
#include "core-engine.h"
#include "core-settings.h"

ObjRenderMgr::ObjRenderMgr()
{

}

ObjRenderMgr::~ObjRenderMgr()
{
	if (obj_res_mgr_)
	{
		delete obj_res_mgr_;
		obj_res_mgr_ = nullptr;
	}
}

bool ObjRenderMgr::InitObjResourceFolder(const char* path)
{
	if (obj_res_mgr_)
		return true;

	obj_res_mgr_ = new ObjResourceMgr();

	if (!obj_res_mgr_->Init(path))
		return false;
	
	if (!InitDiffuseTexture())
		return false;

	if (!InitVSConstantBuffer())
		return false;

	return InitObjVertext();
}

bool ObjRenderMgr::InitDiffuseTexture()
{
	CoreD3D* d3d = core_engine_->GetD3D();
	CoreSettings* settings = core_engine_->GetSettings();

	int width = obj_res_mgr_->diffuse_bitmap_->GetW();
	int height = obj_res_mgr_->diffuse_bitmap_->GetH();;

	d3d->CreateD3DTexture(diffuse_texture_.GetAddressOf(), false, false, width, height, false);
	d3d->CreateShaderResourceView(diffuse_texture_.Get(), diffuse_shaderresource_.GetAddressOf());

	uint8_t* ptr;
	uint32_t linesize_out;
	D3D11_MAPPED_SUBRESOURCE map;
	d3d->Map(diffuse_texture_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
	size_t row_copy;

	ptr = (uint8_t*)map.pData;
	linesize_out = map.RowPitch;
	uint32_t linesize = width * 4;
	uint8_t* data = obj_res_mgr_->diffuse_bitmap_->GetBits();
	row_copy = (linesize < linesize_out) ? linesize : linesize_out;
	if (linesize == linesize_out)
	{
		memcpy(ptr, data, row_copy * height);
	}
	else
	{
		uint8_t* const end = ptr + height * linesize_out;
		while (ptr < end) {
			memcpy(ptr, data, row_copy);
			ptr += linesize_out;
			data += linesize;
		}
	}
	d3d->UnMap(diffuse_texture_.Get(), 0);

	return true;
}


void ObjRenderMgr::ForeachDrawFaces()
{
	UINT strides = sizeof(VertexPosNormalTex);
	UINT offsets = 0;
	CoreD3D* d3d = core_engine_->GetD3D();

	d3d->UpdateVertexShader(CoreD3DData::VertexHlslType::kModel3D);
	d3d->UpdatePixelShader(CoreD3DData::PixelHlslType::kModel3D);
	d3d->GetD3DDeviceContext()->PSSetShaderResources(0, 1, diffuse_shaderresource_.GetAddressOf());
	d3d->GetD3DDeviceContext()->VSSetConstantBuffers(0, 1, m_pConstantBuffers.GetAddressOf());

	d3d->GetD3DDeviceContext()->IASetVertexBuffers(0, 1, vertex_buffer_.GetAddressOf(), &strides, &offsets);
	d3d->GetD3DDeviceContext()->IASetIndexBuffer(index_buffer_.Get(), DXGI_FORMAT_R32_UINT, 0);
	d3d->GetD3DDeviceContext()->DrawIndexed(obj_res_mgr_->face_index_.size(), 0, 0);
}

bool ObjRenderMgr::InitObjVertext()
{
	CoreD3D* d3d = core_engine_->GetD3D();

	D3D11_BUFFER_DESC vbd;
	ZeroMemory(&vbd, sizeof(vbd));
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = obj_res_mgr_->face_vertex_.size() * (UINT)sizeof(VertexPosNormalTex);
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = obj_res_mgr_->face_vertex_.data();
	HR(d3d->GetD3DDevice()->CreateBuffer(&vbd, &InitData, vertex_buffer_.GetAddressOf()));

	D3D11_BUFFER_DESC ibd;
	ZeroMemory(&ibd, sizeof(ibd));
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.ByteWidth = obj_res_mgr_->face_index_.size() * (UINT)sizeof(DWORD);
	InitData.pSysMem = obj_res_mgr_->face_index_.data();
	HR(d3d->GetD3DDevice()->CreateBuffer(&ibd, &InitData, index_buffer_.GetAddressOf()));

	return true;
}

bool ObjRenderMgr::InitVSConstantBuffer()
{
	CoreD3D* d3d = core_engine_->GetD3D();

	D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.ByteWidth = sizeof(VSConstantBuffer);
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	// 新建用于VS和PS的常量缓冲区

	HR(d3d->GetD3DDevice()->CreateBuffer(&cbd, nullptr, m_pConstantBuffers.GetAddressOf()));
	
	m_VSConstantBuffer.world = DirectX::XMMatrixTranspose(DirectX::XMMatrixScaling(1.0f,1.0f,1.0f));
	m_VSConstantBuffer.view = DirectX::XMMatrixTranspose(DirectX::XMMatrixLookAtLH(
		DirectX::XMVectorSet(0.0f, -0.5f, 2.0f, 1.0f),
		DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
		DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f)
	));
	m_VSConstantBuffer.proj = DirectX::XMMatrixTranspose(DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, 8.0f / 6.0f, 1.0f, 500.0f));

	// 更新常量缓冲区，让立方体转起来
	D3D11_MAPPED_SUBRESOURCE mappedData;
	HR(d3d->GetD3DDeviceContext()->Map(m_pConstantBuffers.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));
	memcpy_s(mappedData.pData, sizeof(VSConstantBuffer), &m_VSConstantBuffer, sizeof(VSConstantBuffer));
	d3d->GetD3DDeviceContext()->Unmap(m_pConstantBuffers.Get(), 0);
	d3d->GetD3DDeviceContext()->VSSetConstantBuffers(0, 1, m_pConstantBuffers.GetAddressOf());
	return true;
}