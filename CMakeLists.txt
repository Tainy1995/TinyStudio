cmake_minimum_required (VERSION 3.8)

project(TinyStudio)

set(TARGET_NAME TinyStudio)

set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_compile_definitions(UNICODE _UNICODE)

set(CMAKE_CXX_STANDARD 17)

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO /NODEFAULTLIB:libc.lib")	

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(_win_version "x64")
else()
	set(_win_version "x86")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${PROJECT_ROOT_DIR}/bin/${_win_version}/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${PROJECT_ROOT_DIR}/bin/${_win_version}/Release)   
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${PROJECT_ROOT_DIR}/bin/${_win_version}/Debug)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${PROJECT_ROOT_DIR}/bin/${_win_version}/Release)


set(INCLUDE_DIRECTORY
	${CMAKE_CURRENT_SOURCE_DIR}/third-part/ffmpeg/include
	${CMAKE_CURRENT_SOURCE_DIR}/third-part/sdl/include
	${CMAKE_CURRENT_SOURCE_DIR}/third-part/libyuv
	${CMAKE_CURRENT_SOURCE_DIR}/third-part/libfacedetection/include
	${CMAKE_CURRENT_SOURCE_DIR}/third-part/x264/include
	${CMAKE_CURRENT_SOURCE_DIR}/third-part/nlohmann
	${CMAKE_CURRENT_SOURCE_DIR}/third-part/libfacedetection/include
	${CMAKE_CURRENT_SOURCE_DIR}/app/core
	${CMAKE_CURRENT_SOURCE_DIR}/app/utils
	${CMAKE_CURRENT_SOURCE_DIR}/app/dx
	${CMAKE_CURRENT_SOURCE_DIR}/app/sources
	${CMAKE_CURRENT_SOURCE_DIR}/app/sources/desktop
	${CMAKE_CURRENT_SOURCE_DIR}/app/sources/gdiplus-text
	${CMAKE_CURRENT_SOURCE_DIR}/app/sources/media
	${CMAKE_CURRENT_SOURCE_DIR}/app/sources/model
	${CMAKE_CURRENT_SOURCE_DIR}/app/sources/model/obj-model
	${CMAKE_CURRENT_SOURCE_DIR}/app/sources/image
	${CMAKE_CURRENT_SOURCE_DIR}/app/encoder
	${CMAKE_CURRENT_SOURCE_DIR}/app/output
	${CMAKE_CURRENT_SOURCE_DIR}/app/encoder/x264-encoder
	${CMAKE_CURRENT_SOURCE_DIR}/app/encoder/ffmpeg-aac-encoder
	${CMAKE_CURRENT_SOURCE_DIR}/app/output/file-output
	${CMAKE_CURRENT_SOURCE_DIR}/app/filter
	${CMAKE_CURRENT_SOURCE_DIR}/app/filter/split
	${CMAKE_CURRENT_SOURCE_DIR}/app/filter/bolt-box
	${CMAKE_CURRENT_SOURCE_DIR}/app/filter/ai-detect-mgr
	${CMAKE_CURRENT_SOURCE_DIR}/app/audio

)
include_directories(${INCLUDE_DIRECTORY})

set(CORE_SOURCES
	app/core/main.cc
	app/core/core-engine.h
	app/core/core-engine.cc
	app/core/core-component-i.h
	app/core/core-display.h
	app/core/core-display.cc
	app/core/core-settings.h
	app/core/core-settings.cc
	app/core/core-settings-data.h
	app/core/core-audio-data.h
	app/core/core-scene.h
	app/core/core-scene.cc
	app/core/core-video.h
	app/core/core-video.cc
	app/core/core-audio.h
	app/core/core-audio.cc
	app/core/core-d3d.h
	app/core/core-d3d.cc
	app/core/core-output.h
	app/core/core-output.cc
	app/core/core-filter.h
	app/core/core-filter.cc
)

set(UTILS
	app/utils/logger.h
	app/utils/logger.cc
	app/utils/platform.h
	app/utils/platform.cc
	app/utils/circlebuf.h
)

set(SOURCES_SRC
	app/sources/base-source-i.h
	app/sources/base-source-i.cc
)

set(AUDIO_SRC
	app/audio/audio-resampler.h
	app/audio/audio-resampler.cc
	app/audio/wasapi-audio-monitor.h
	app/audio/wasapi-audio-monitor.cc
	app/audio/wasapi-audio-source.h
	app/audio/wasapi-audio-source.cc

)

set(DX_SOURCES
	app/dx/d3dUtil.h
	app/dx/d3dUtil.cpp
	app/dx/dx-header.h
	app/dx/dx-header.cc
	app/dx/DXTrace.h
	app/dx/DXTrace.cpp
	app/dx/Geometry.h
	app/dx/Vertex.h
	app/dx/Vertex.cpp

)

set(SOURCES_DESKTOPCAPTURE
	app/sources/desktop/desktop-capture.h
	app/sources/desktop/desktop-capture.cc

)

set(SOURCES_gdiplustext
	app/sources/gdiplus-text/gdiplus-text-source.h
	app/sources/gdiplus-text/gdiplus-text-source.cc

)

set(SOURCES_MEDIA
	app/sources/media/media-source-logic.h
	app/sources/media/media-source-logic.cc
	app/sources/media/media-controler.h
	app/sources/media/media-controler.cc

)

set(SOURCES_MODEL_OBJMODEL
	app/sources/model/obj-model/obj-resource-mgr.h
	app/sources/model/obj-model/obj-resource-mgr.cc
	app/sources/model/obj-model/obj-render-mgr.h
	app/sources/model/obj-model/obj-render-mgr.cc
	app/sources/model/obj-model/obj-model-source.h
	app/sources/model/obj-model/obj-model-source.cc
	app/sources/model/obj-model/bitmap-texture-file.h

)

set(ENCODER_SRC
	app/encoder/base-encoder-data.h
	app/encoder/base-encoder-i.h

)

set(ENCODER_X264ENCODER
	app/encoder/x264-encoder/x264-encoder.h
	app/encoder/x264-encoder/x264-encoder.cc
)

set(ENCODER_FFMPEGAACENCODER
	app/encoder/ffmpeg-aac-encoder/ffmpeg-aac-encoder.h
	app/encoder/ffmpeg-aac-encoder/ffmpeg-aac-encoder.cc
)

set(OUTPUT_FILEOUTPUT
	app/output/file-output/file-output.h
	app/output/file-output/file-output.cc
)

set(OUTPUT_SRC
	app/output/base-output-i.h
)

set(FILTER_SRC
	app/filter/base-filter-i.h
)

set(FILTER_SPLIT
	app/filter/split/split-filter.h
	app/filter/split/split-filter.cc
)

set(FILTER_BOLTBOX
	app/filter/bolt-box/boltbox-filter.h
	app/filter/bolt-box/boltbox-filter.cc
)

set(SOURCE_IMAGE
	app/sources/image/graphics-ffmpeg.h
	app/sources/image/graphics-ffmpeg.cc
	app/sources/image/image-source.h
	app/sources/image/image-source.cc
)

set(FILTER_AI_DETECT_MGR
	app/filter/ai-detect-mgr/ai-detect-define.h
	app/filter/ai-detect-mgr/ai-detect-item-i.h
	app/filter/ai-detect-mgr/ai-detect-item-i.cc
	app/filter/ai-detect-mgr/ai-detect-mgr.h
	app/filter/ai-detect-mgr/ai-detect-mgr.cc
	app/filter/ai-detect-mgr/item-face-cnn.h
	app/filter/ai-detect-mgr/item-face-cnn.cc
)

set(FILTER_FACEDETECT
	app/filter/face-detect/facedetect-filter.h
	app/filter/face-detect/facedetect-filter.cc
)

source_group("core" FILES ${CORE_SOURCES})
source_group("utils" FILES ${UTILS})
source_group("sources" FILES ${SOURCES_SRC})
source_group("dx" FILES ${DX_SOURCES})
source_group("encoder" FILES ${ENCODER_SRC})
source_group("output" FILES ${OUTPUT_SRC})
source_group(sources\\\\desktop FILES ${SOURCES_DESKTOPCAPTURE})
source_group(sources\\\\gdiplus-text FILES ${SOURCES_gdiplustext})
source_group(sources\\\\meida FILES ${SOURCES_MEDIA})
source_group(sources\\\\model\\\\obj-model FILES ${SOURCES_MODEL_OBJMODEL})
source_group(sources\\\\image FILES ${SOURCE_IMAGE})
source_group(encoder\\\\x264-encoder FILES ${ENCODER_X264ENCODER})
source_group(encoder\\\\ffmpegaac-encode FILES ${ENCODER_FFMPEGAACENCODER})
source_group(output\\\\flv-output FILES ${OUTPUT_FILEOUTPUT})
source_group("filter" FILES ${FILTER_SRC})
source_group(filter\\\\split FILES ${FILTER_SPLIT})
source_group(filter\\\\boltbox FILES ${FILTER_BOLTBOX})
source_group(filter\\\\ai-detect-mgr FILES ${FILTER_AI_DETECT_MGR})
source_group(filter\\\\face-detect FILES ${FILTER_FACEDETECT})
source_group("audio" FILES ${AUDIO_SRC})

add_executable(${TARGET_NAME} WIN32
	${CORE_SOURCES}
	${UTILS}
	${SOURCES_SRC}
	${SOURCES_DESKTOPCAPTURE}
	${DX_SOURCES}
	${ENCODER_SRC}
	${OUTPUT_SRC}
	${ENCODER_X264ENCODER}
	${OUTPUT_FILEOUTPUT}
	${SOURCES_gdiplustext}
	${SOURCES_MEDIA}
	${SOURCES_MODEL_OBJMODEL}
	${ENCODER_FFMPEGAACENCODER}
	${FILTER_SRC}
	${FILTER_SPLIT}
	${FILTER_BOLTBOX}
	${AUDIO_SRC}
	${SOURCE_IMAGE}
	${FILTER_AI_DETECT_MGR}
	${FILTER_FACEDETECT}
)

set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME ${TARGET_NAME})

set(target_links
	d2d1
    d3d11
	dxgi
	dxguid
	D3DCompiler
	winmm
	gdiplus
	strmiids
	ksuser
	wmcodecdspuuid
	${CMAKE_CURRENT_SOURCE_DIR}/third-part/ffmpeg/${_win_version}/avcodec.lib
	${CMAKE_CURRENT_SOURCE_DIR}/third-part/ffmpeg/${_win_version}/avformat.lib
	${CMAKE_CURRENT_SOURCE_DIR}/third-part/ffmpeg/${_win_version}/avutil.lib
	${CMAKE_CURRENT_SOURCE_DIR}/third-part/ffmpeg/${_win_version}/swscale.lib
	${CMAKE_CURRENT_SOURCE_DIR}/third-part/ffmpeg/${_win_version}/swresample.lib
	${CMAKE_CURRENT_SOURCE_DIR}/third-part/ffmpeg/${_win_version}/avdevice.lib
	${CMAKE_CURRENT_SOURCE_DIR}/third-part/libyuv/${_win_version}/yuv.lib
	${CMAKE_CURRENT_SOURCE_DIR}/third-part/x264/${_win_version}/x264.lib
	debug ${CMAKE_CURRENT_SOURCE_DIR}/third-part/libfacedetection/${_win_version}/facedetectiond.lib
	optimized ${CMAKE_CURRENT_SOURCE_DIR}/third-part/libfacedetection/${_win_version}/facedetection.lib
)

target_link_libraries(${TARGET_NAME} ${target_links})

set(BIN_HLSL_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/${_win_version}/$<CONFIG>/HLSL)
set(BIN_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/${_win_version}/$<CONFIG>)
set(BIN_MODEL_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/${_win_version}/$<CONFIG>/model)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
COMMAND ${CMAKE_COMMAND} -E make_directory ${BIN_HLSL_DIRECTORY}
COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources/HLSL ${BIN_HLSL_DIRECTORY}
COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/third-part/ffmpeg/${_win_version} ${BIN_DIRECTORY}
COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources/images ${BIN_DIRECTORY}
COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources/videos ${BIN_DIRECTORY}
COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources/model ${BIN_MODEL_DIRECTORY}
COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/third-part/libyuv/${_win_version} ${BIN_DIRECTORY}
COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/third-part/x264/${_win_version} ${BIN_DIRECTORY}
)
