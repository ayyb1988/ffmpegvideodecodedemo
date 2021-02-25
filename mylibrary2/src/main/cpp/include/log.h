//
// Created by yabin on 2021/2/24.
//

#ifndef FFMPEGDEMO2_LOG_H
#define FFMPEGDEMO2_LOG_H

extern "C"{
#include <android/log.h>
};


#ifndef LOG_TAG
#define LOG_TAG "JNI"
#endif

#define FFMPEG_LOG_TAG	"FFmpeg_Native"



#define LOGV(...)__android_log_print(ANDROID_LOG_VERBOSE, FFMPEG_LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG ,FFMPEG_LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO  ,FFMPEG_LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN  ,FFMPEG_LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR ,FFMPEG_LOG_TAG, __VA_ARGS__)


#endif //FFMPEGDEMO2_LOG_H
