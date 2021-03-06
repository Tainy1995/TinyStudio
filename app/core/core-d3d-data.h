#pragma once

namespace CoreD3DData
{
	enum class VertexHlslType
	{
		kUnknown = 0,
		kBasic2D,
		kExpand2D,
		kScale2D,
		kModel3D,
	};

	enum class PixelHlslType
	{
		kUnknown = 0,
		kBasic2D,
		kAlpha2D,
		kModel3D,
		kThreeMirror2D,
	};

}