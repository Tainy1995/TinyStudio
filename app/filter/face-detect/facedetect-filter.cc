#include "facedetect-filter.h"
#include "facedetectcnn.h"
#include "core-d3d.h"
#include "core-engine.h"
#include "logger.h"
#include <chrono>

FaceDetectFilter::FaceDetectFilter()
{

}

FaceDetectFilter::~FaceDetectFilter()
{

}

bool FaceDetectFilter::Init()
{
    ai_detect_mgr_.InitAiDetectMgr({ AiDetect::AiDetectInput(AiDetect::AiDetectType::KAiDetectFace, AiDetect::AiDetectThreadType::KAiDetectIndependThread, 30) });
    init_result_ = true;
    return true;
}

void FaceDetectFilter::Update(const char* jsondata)
{

}

void FaceDetectFilter::SetInputTexture(ID3D11Texture2D* texture, size_t width, size_t height, uint8_t* data)
{
    CoreD3D* d3d = core_engine_->GetD3D();
    if (!d3d)
        return;

    if (!InitResource(width, height))
        return;

    ComPtr<ID3D11RenderTargetView> View;
    ComPtr<ID3D11DepthStencilView> Depth;
    d3d->OMGetRenderTargets(1, View.GetAddressOf(), Depth.GetAddressOf());

    d3d->CopyResource(input_texture_.Get(), texture);

    uint8_t* scale_data = nullptr;

    {
        static float color[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
        d3d->PushFrontViewPort(scale_width_, scale_height_);
        d3d->UpdateVertexShader(CoreD3DData::VertexHlslType::kBasic2D);
        d3d->UpdatePixelShader(CoreD3DData::PixelHlslType::kBasic2D);
        d3d->OMSetRenderTargets(1, input_scale_resource_view_.GetAddressOf(), nullptr);
        d3d->ClearRenderTargetView(input_scale_resource_view_.Get(), color);
        UINT buffersize = 0;
        auto meshData = Geometry::Create2DShow();
        d3d->ResetMesh(meshData, buffersize);
        d3d->PSSetShaderResources(0, 1, input_texture_resource_view_.GetAddressOf());
        d3d->DrawIndexed(buffersize, 0, 0);
        d3d->PopFrontViewPort();
        d3d->CopyResource(input_scale_read_texture_.Get(), input_scale_texture_.Get());

        D3D11_MAPPED_SUBRESOURCE map;
        d3d->Map(input_scale_read_texture_.Get(), 0, D3D11_MAP_READ, 0, &map);
        char* texture_data = (char*)map.pData;
        int dataSize = map.DepthPitch;
        scale_data = (uint8_t*)malloc(dataSize);
        memcpy(scale_data, texture_data, dataSize);
        d3d->UnMap(input_scale_read_texture_.Get(), 0);
    }

    if (!scale_data)
        return;

    ai_detect_mgr_.InputBgraRawPixelData(scale_data, scale_width_ * scale_height_ * 4, scale_width_, scale_height_);
    free(scale_data);

    float scaleX = width / (float)scale_width_;
    float scaleY = height / (float)scale_height_;

    float dpix = 0;
    float dpiy = 0;
    paint_render_target_->GetDpi(&dpix, &dpiy);

    dpix = dpix != 0 ? dpix / 96.0f : 1.0f;
    dpiy = dpiy != 0 ? dpiy / 96.0f : 1.0f;

    std::vector< AiDetect::AiDetectResult > aiResult;
    ai_detect_mgr_.GetOutputDetectResult(aiResult);
    d3d->PushFrontViewPort(width, height);
    paint_render_target_->BeginDraw();
    paint_render_target_->Clear(D2D1::ColorF(D2D1::ColorF::White, 0.0f));
    if (!pen_brush)
    {
        HRESULT hr = paint_render_target_->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::DarkRed, 1.0f), pen_brush.GetAddressOf());
    }
    for (const auto& c : aiResult)
    {
        if (c.detectType == AiDetect::AiDetectType::KAiDetectFace)
        {
            for (int i = 0; i < c.faceResult.size; i++)
            {
                float x = c.faceResult.info[i].x * scaleX / dpix;
                float y = c.faceResult.info[i].y * scaleY / dpiy;
                float w = c.faceResult.info[i].width * scaleX / dpix;
                float h = c.faceResult.info[i].height * scaleY / dpiy;
                float confidence = c.faceResult.info[i].confidence;

                if (confidence <= 75)
                    continue;

                //LOGGER_INFO("%f %f %f %f", x, y, w, h);

                paint_render_target_->DrawLine(
                    D2D1::Point2F(x, y),
                    D2D1::Point2F(x + w, y), pen_brush.Get(),
                    3.0f, stroke_style_.Get());

                paint_render_target_->DrawLine(
                    D2D1::Point2F(x + w, y),
                    D2D1::Point2F(x + w, y + h), pen_brush.Get(),
                    3.0f, stroke_style_.Get());

                paint_render_target_->DrawLine(
                    D2D1::Point2F(x + w, y + h),
                    D2D1::Point2F(x, y + h), pen_brush.Get(),
                    3.0f, stroke_style_.Get());

                paint_render_target_->DrawLine(
                    D2D1::Point2F(x, y + h),
                    D2D1::Point2F(x, y), pen_brush.Get(),
                    3.0f, stroke_style_.Get());

                //for (int j = 0; j < 10; j += 2)
                //{
                //    D2D1_POINT_2F center_point;
                //    center_point.x = c.faceResult.info[i].landmark[j] * scaleX;
                //    center_point.y = c.faceResult.info[i].landmark[j+1] * scaleY;
                //    paint_render_target_->DrawEllipse(D2D1_ELLIPSE{ center_point, 2, 2 },
                //        pen_brush.Get(), 2, stroke_style_.Get());
                //}
            }
        }
    }

    paint_render_target_->EndDraw();

    d3d->CopyResource(copy_paint_texture_.Get(), paint_texture_.Get());

    UINT buffersize = 0;
    auto meshData = Geometry::Create2DShow();
    d3d->ResetMesh(meshData, buffersize);
    static float color[4] = { 0.0f, 1.0f, 0.0f, 0.0f };

    d3d->OMSetRenderTargets(1, face_rendertarget_view_.GetAddressOf(), nullptr);
    d3d->ClearRenderTargetView(face_rendertarget_view_.Get(), color);

    d3d->OpenAlphaBlend();
    d3d->PSSetShaderResources(0, 1, input_texture_resource_view_.GetAddressOf());
    d3d->DrawIndexed(buffersize, 0, 0);

    d3d->PSSetShaderResources(0, 1, copy_paint_texture_resource_view_.GetAddressOf());
    d3d->DrawIndexed(buffersize, 0, 0);
    d3d->CloseAlphaBlend();
    d3d->PopFrontViewPort();
    d3d->OMSetRenderTargets(1, View.GetAddressOf(), Depth.Get());
}

void FaceDetectFilter::CopyOutputTexture(ID3D11Texture2D* dst)
{
    CoreD3D* d3d = core_engine_->GetD3D();
    if (!d3d)
        return;

    if (face_render_target_)
        d3d->CopyResource(dst, face_render_target_.Get());
}

bool FaceDetectFilter::InitResource(size_t width, size_t height)
{
    CoreD3D* d3d = core_engine_->GetD3D();
    if (!d3d)
        return false;

    if (!factory_)
    {
        HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, (ID2D1Factory**)factory_.GetAddressOf()));
    }
    
    if (!paint_texture_)
    {
        if(!d3d->CreateD3DTexture(paint_texture_.GetAddressOf(), true, true, width, height, false))
            return false;

        HR(paint_texture_->QueryInterface(__uuidof(IDXGISurface1), (void**)dxgi_surface_.GetAddressOf()));

        factory_->GetDesktopDpi(&dpi_x_, &dpi_y_);
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
            dpi_x_, dpi_y_);

        HR(factory_->CreateDxgiSurfaceRenderTarget(dxgi_surface_.Get(), &props, paint_render_target_.GetAddressOf()));

        paint_render_target_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

        HR(factory_->CreateStrokeStyle(
            D2D1::StrokeStyleProperties(
                D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND, 20.0f,
                D2D1_DASH_STYLE_SOLID, 0.0f),
            nullptr, 0, stroke_style_.GetAddressOf()));

        D2D1_BITMAP_PROPERTIES bitmap_properties;
        bitmap_properties.dpiX = dpi_x_;
        bitmap_properties.dpiY = dpi_y_;
        bitmap_properties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
        bitmap_properties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
        paint_render_target_->CreateBitmap(D2D1_SIZE_U{ (uint32_t)width, (uint32_t)height }, nullptr,
            width * 4, bitmap_properties, // bgra
            render_target_bitmap_.GetAddressOf());

        HR(paint_render_target_->CreateSolidColorBrush(D2D1::ColorF(0x008000, 1.0f), rect_brush_.GetAddressOf()));
    }
    if (!face_render_target_)
    {
        if (!d3d->CreateD3DTexture(face_render_target_.GetAddressOf(), true, false, width, height,false))
            return false;
        if (!d3d->CreateRenderTargetView(face_render_target_.Get(), face_rendertarget_view_.GetAddressOf()))
            return false;
    }

    if (!input_texture_)
    {
        if (!d3d->CreateD3DTexture(input_texture_.GetAddressOf(), false, false, width, height,false))
            return false;

        if (!d3d->CreateShaderResourceView(input_texture_.Get(), input_texture_resource_view_.GetAddressOf()))
            return false;
    }

    if (!copy_paint_texture_)
    {
        if (!d3d->CreateD3DTexture(copy_paint_texture_.GetAddressOf(), false, false, width, height,false))
            return false;

        if (!d3d->CreateShaderResourceView(copy_paint_texture_.Get(), copy_paint_texture_resource_view_.GetAddressOf()))
            return false;
    }

    if (!input_scale_texture_)
    {
        float scale_pix = (float)width / (float)height;
        scale_height_ = (float)scale_width_ / scale_pix; 

        if (!d3d->CreateD3DTexture(input_scale_texture_.GetAddressOf(), true, false, scale_width_, scale_height_, false))
            return false;

        if (d3d->CreateRenderTargetView(input_scale_texture_.Get(), input_scale_resource_view_.GetAddressOf()))
            return false;
    }

    if (!input_scale_read_texture_)
    {
        if (d3d->CreateD3DTexture(input_scale_read_texture_.GetAddressOf(), false, false, scale_width_, scale_height_,true))
            return false;
    }
    return true;
}

