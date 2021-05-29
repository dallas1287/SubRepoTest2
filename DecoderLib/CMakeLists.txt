add_library(DecoderLib MediaReaderState.cpp MediaConverter.cpp decoder.cxx)

set(FFMPEG_INCLUDE_DIR /home/dallas/ffmpeg/wasm/include)
set(FFMPEG_LIB_DIR /home/dallas/ffmpeg/wasm/lib)

find_path(AVCODEC_INCLUDE_DIR libavcodec ${FFMPEG_INCLUDE_DIR})
find_library(AVCODEC_LIB libavcodec.a ${FFMPEG_LIB_DIR})
find_library(AVFORMAT_LIB libavformat.a ${FFMPEG_LIB_DIR})
find_library(AVFILTER_LIB libavfilter.a ${FFMPEG_LIB_DIR})
find_library(AVUTIL_LIB libavutil.a ${FFMPEG_LIB_DIR})
find_library(SWSCALE_LIB libswscale.a ${FFMPEG_LIB_DIR})
find_library(SWRESAMPLE_LIB libswresample.a ${FFMPEG_LIB_DIR})
find_library(AVDEVICE_LIB libavdevice.a ${FFMPEG_LIB_DIR})
message(${AVCODEC_INCLUDE_DIR})

message(${AVCODEC_LIB})
message(${AVFORMAT_LIB})
message(${AVFILTER_LIB})
message(${AVUTIL_LIB})
message(${SWSCALE_LIB})
message(${SWRESAMPLE_LIB})
message(${AVDEVICE_LIB})

target_include_directories(DecoderLib PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}
${AVCODEC_INCLUDE_DIR}
)

target_link_libraries(DecoderLib PUBLIC
${AVCODEC_LIB}
${AVFORMAT_LIB}
${AVFILTER_LIB}
${AVUTIL_LIB}
${SWSCALE_LIB}
${SWRESAMPLE_LIB}
${AVDEVICE_LIB}
#Threads::Threads
)

install(TARGETS DecoderLib DESTINATION lib)
install(FILES decoder.h DESTINATION include)