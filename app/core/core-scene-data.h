#pragma once

namespace CoreSceneData
{
	enum class SourceType
	{
		kSourceDesktop,
		kSourceImage,
		kSourceGame,
		kSourceGdiplusText,
		kSourceMedia,
		kSourceCamera,
		kSourceObjModel
	};

	enum class FilterType
	{
		kFilterFaceDetect,
		kFilterSplit,
		kFilterBoltBox,
	};

	enum class AlignType
	{
		kAlignLeft = -4,
		kAlignRight = -3,
		kAlignTop = -2,
		kAlignBottom = -1,
	};

	enum class SizeType
	{
		kClientSize = -2,
		kActuallySize = -1,
	};
}