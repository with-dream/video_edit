cmake_minimum_required(VERSION 3.17)

set(pro video_edit)

project(${pro})

set(CMAKE_CXX_STANDARD 14)
set(ffmpeg_path /Users/ms/share/project/modules/ffmpeg/ffmpeg)

include_directories(
        /usr/local/include
        ./
)

link_directories(
        /usr/local/lib/
)

add_executable(${pro}
        main.cpp
        Slice.cpp Slice.h)

target_link_libraries(${pro}
        avfilter
        avutil
        avdevice
        avformat
        swscale
        avcodec
        swresample
        )