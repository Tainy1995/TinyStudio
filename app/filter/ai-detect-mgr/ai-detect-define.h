#ifndef AI_DETECT_DEFINE_H
#define AI_DETECT_DEFINE_H

#include <vector>
#include <string>

namespace AiDetect
{

	enum AiDetectThreadType
	{
		KAiDetectRenderThread = 0,
		KAiDetectIndependThread,
	};

	enum AiDetectType
	{
		KAiDetectFace = 0,
	};

	struct AiDetectInput
	{
		AiDetectType detectType;
		AiDetectThreadType threadType;
		int detectFrameRate;
		AiDetectInput()
		{

		}
		AiDetectInput(AiDetectType detecttype, AiDetectThreadType threadtype, int framerate)
		{
			detectType = detecttype;
			threadType = threadtype;
			detectFrameRate = framerate;
		}
	};

	struct FaceDetectionCnn
	{
		short x = 0;
		short y = 0;
		short width = 0;
		short height = 0;
		short confidence = 0;
		short landmark[10] = { 0 };
		FaceDetectionCnn()
		{

		}
		FaceDetectionCnn(const FaceDetectionCnn& other)
		{
			x = other.x;
			y = other.y;
			width = other.width;
			height = other.height;
			confidence = other.confidence;
			for (int i = 0; i < 10; i++)
			{
				landmark[i] = other.landmark[i];
			}
		}
	};

	struct FaceInfo
	{
		FaceDetectionCnn* info = nullptr;
		short size = 0;
		FaceInfo()
		{
			info = nullptr;
			size = 0;
		}
		~FaceInfo()
		{
			Clear();
		}
		void Clear()
		{
			if (size && info)
			{
				delete[] info;
				info = nullptr;
			}
			size = 0;
		}
		void Alloc(short _size)
		{
			Clear();
			size = _size;
			if(size)
				info = new FaceDetectionCnn[size];
		}
	};

	struct AiDetectResult
	{
		AiDetectType detectType = AiDetectType::KAiDetectFace;

		FaceInfo faceResult;

		void Clear()
		{
			switch (detectType)
			{
			case KAiDetectFace:
			{
				faceResult.Clear();
			}
			break;
			}
		}
		AiDetectResult()
		{
			Clear();
		}

		~AiDetectResult()
		{
			Clear();
		}

		AiDetectResult& operator=(AiDetectResult const& other)
		{
			detectType = other.detectType;
			switch (detectType)
			{
			case KAiDetectFace:
			{
				faceResult.Alloc(other.faceResult.size);
				for (int i = 0; i < faceResult.size; i++)
				{
					faceResult.info[i] = other.faceResult.info[i];
				}
			}
			break;
			}
			return *this;
		}

		AiDetectResult(const AiDetectResult& other)
		{
			detectType = other.detectType;
			switch (detectType)
			{
			case KAiDetectFace:
			{
				faceResult.Alloc(other.faceResult.size);
				for (int i = 0; i < faceResult.size; i++)
				{
					faceResult.info[i] = other.faceResult.info[i];
				}
			}
			break;
			}
		}
	};
}

#endif