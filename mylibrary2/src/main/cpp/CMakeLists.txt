
cmake_minimum_required(VERSION 3.10.2)


project("ffmpegdemo2")

include_directories(${PROJECT_SOURCE_DIR}/include)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${CMAKE_SOURCE_DIR}/../jniLibs/${CMAKE_ANDROID_ARCH_ABI}")

add_library( # Sets the name of the library.
        native-lib

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        native-lib.cpp)


find_library( # Sets the name of the path variable.
        log-lib

        log)


target_link_libraries( # Specifies the target library.
        native-lib
        avformat
        avcodec
        avfilter
        avutil
        swresample
        swscale
        OpenSLES

        ${log-lib})