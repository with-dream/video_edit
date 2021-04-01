cmake_minimum_required(VERSION 3.17)

set(pro video_edit)

project(${pro})

set(CMAKE_CXX_STANDARD 14)

include_directories(
        /usr/local/include
        ./
)

link_directories(
        /usr/local/lib/
)

set(src_list
#        examples/decode_video.c
#        examples/transcoding.c
        main.cpp
#        tools/Slice.cpp
#        tools/Transcode.cpp
        D.cpp D.h)

add_executable(${pro}
        ${src_list})

target_link_libraries(${pro}
        avfilter
        avutil
        avdevice
        avformat
        swscale
        avcodec
        swresample
        )