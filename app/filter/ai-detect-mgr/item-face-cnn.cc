#include "item-face-cnn.h"
#include "facedetectcnn.h"
#include "facedetection_export.h"

ItemFaceCnn::ItemFaceCnn()
{
    is_detecting_.store(false);
	result_buffer_.resize(0x20000);
    face_result_.detectType = AiDetect::AiDetectType::KAiDetectFace;
    face_result_.faceResult.size = 0;
    face_result_.faceResult.info = nullptr;
}

ItemFaceCnn::~ItemFaceCnn()
{

}

void ItemFaceCnn::InputBgraRawPixelDataImpl(uint8_t* data, size_t size, size_t width, size_t height)
{
    auto _BgraToBgr = [](uint8_t* bgraData, int bgraSize, uint8_t* bgrData) {
        for (int i = 0, j = 0; j < bgraSize; i += 3, j += 4) {
            *(bgrData + i) = *(bgraData + j);
            *(bgrData + i + 1) = *(bgraData + j + 1);
            *(bgrData + i + 2) = *(bgraData + j + 2);
        }
    };

    is_detecting_.store(true);

    std::shared_ptr<uint8_t> bgr_buffer(new uint8_t[width * height * 3], std::default_delete<uint8_t[]>());
    _BgraToBgr(data, width * height * 4, bgr_buffer.get());
    result_buffer_.clear();
    int* result = facedetect_cnn((unsigned char*)&result_buffer_[0], bgr_buffer.get(), width, height, width * 3);

    is_detecting_.store(false);

    std::unique_lock<std::mutex> lock(mutex_);
    if (result)
    {
        face_result_.faceResult.Alloc(*result);
        face_result_.faceResult.size = *result;
        for (int i = 0; i < *result; ++i) {
            short* p = ((short*)(result + 1)) + 142 * i;
            face_result_.faceResult.info[i].confidence = p[0];
            face_result_.faceResult.info[i].x = p[1];
            face_result_.faceResult.info[i].y = p[2];
            face_result_.faceResult.info[i].width = p[3];
            face_result_.faceResult.info[i].height = p[4];
            face_result_.faceResult.info[i].landmark[0] = p[5];
            face_result_.faceResult.info[i].landmark[1] = p[6];
            face_result_.faceResult.info[i].landmark[2] = p[7];
            face_result_.faceResult.info[i].landmark[3] = p[8];
            face_result_.faceResult.info[i].landmark[4] = p[9];
            face_result_.faceResult.info[i].landmark[5] = p[10];
            face_result_.faceResult.info[i].landmark[6] = p[11];
            face_result_.faceResult.info[i].landmark[7] = p[12];
            face_result_.faceResult.info[i].landmark[8] = p[13];
            face_result_.faceResult.info[i].landmark[9] = p[14];
        }
    }
    else
    {
        face_result_.faceResult.Clear();
    }


}

void ItemFaceCnn::GetOutputDetectResultImpl(AiDetect::AiDetectResult& result)
{
    if (is_detecting_.load())
    {
        result = face_result_;
    }
    else
    {
        std::unique_lock<std::mutex> lock(mutex_);
        result = face_result_;
    }
}

bool ItemFaceCnn::IsCurDetecting()
{
    return is_detecting_.load();
}
