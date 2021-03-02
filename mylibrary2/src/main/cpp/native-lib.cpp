#include <jni.h>
#include <string>
#include <unistd.h>


extern "C" {
#include "include/libavcodec/avcodec.h"
#include "include/libavformat/avformat.h"
#include "include/log.h"
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>

}


extern "C" JNIEXPORT jstring JNICALL
Java_android_spport_mylibrary2_Demo_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    const char *string = av_version_info();
    return env->NewStringUTF(string);
}


extern "C"
JNIEXPORT jint JNICALL
Java_android_spport_mylibrary2_Demo_decodeVideo(JNIEnv *env, jobject thiz, jstring inputPath,
                                                jstring outPath) {
    //申请avFormatContext空间，记得要释放
    AVFormatContext *avFormatContext = avformat_alloc_context();


    const char *url = env->GetStringUTFChars(inputPath, 0);

    //1. 打开媒体文件
    int reuslt = avformat_open_input(&avFormatContext, url, NULL, NULL);
    if (reuslt != 0) {
        LOGE("open input error url=%s, result=%d", url, reuslt);
        return -1;
    }
    //2.读取媒体文件信息，给avFormatContext赋值
    if (avformat_find_stream_info(avFormatContext, NULL) < 0) {
        LOGE("find stream error");
        return -1;
    }

    //3. 匹配到视频流的index
    int videoIndex = -1;
    for (int i = 0; i < avFormatContext->nb_streams; i++) {
        AVMediaType codecType = avFormatContext->streams[i]->codecpar->codec_type;
        LOGI("avcodec type %d", codecType);
        if (AVMEDIA_TYPE_VIDEO == codecType) {
            videoIndex = i;
            break;
        }
    }
    if (videoIndex == -1) {
        LOGE("not find a video stream");
        return -1;
    }

    AVCodecParameters *pCodecParameters = avFormatContext->streams[videoIndex]->codecpar;

    //4. 根据视频流信息的codec_id找到对应的解码器
    AVCodec *pCodec = avcodec_find_decoder(pCodecParameters->codec_id);

    if (pCodec == NULL) {
        LOGE("Couldn`t find Codec");
        return -1;
    }

    AVCodecContext *pCodecContext = avFormatContext->streams[videoIndex]->codec;

    //5.使用给定的AVCodec初始化AVCodecContext
    int openResult = avcodec_open2(pCodecContext, pCodec, NULL);
    if (openResult < 0) {
        LOGE("avcodec open2 result %d", openResult);
        return -1;
    }

    const char *outPathStr = env->GetStringUTFChars(outPath, NULL);

    //6. 初始化输出文件、解码AVPacket和AVFrame结构体

    //新建一个二进制文件，已存在的文件将内容清空，允许读写
    FILE *pYUVFile = fopen(outPathStr, "wb+");
    if (pYUVFile == NULL) {
        LOGE(" fopen outPut file error");
        return -1;
    }


    auto *packet = (AVPacket *) av_malloc(sizeof(AVPacket));

    //avcodec_receive_frame时作为参数，获取到frame，获取到的frame有些可能是错误的要过滤掉，否则相应帧可能出现绿屏
    AVFrame *pFrame = av_frame_alloc();
    //作为yuv输出的frame承载者，会进行缩放和过滤出错的帧，YUV相应的数据也是从该对象中读取
    AVFrame *pFrameYUV = av_frame_alloc();

    //out_buffer中数据用于渲染的,且格式为YUV420P
    uint8_t *out_buffer = (unsigned char *) av_malloc(
            av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecContext->width,
                                     pCodecContext->height, 1));

    av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer,
                         AV_PIX_FMT_YUV420P, pCodecContext->width, pCodecContext->height, 1);

    // 由于解码出来的帧格式不一定是YUV420P的,在渲染之前需要进行格式转换
    struct SwsContext *img_convert_ctx = sws_getContext(pCodecContext->width, pCodecContext->height,
                                                        pCodecContext->pix_fmt,
                                                        pCodecContext->width, pCodecContext->height,
                                                        AV_PIX_FMT_YUV420P,
                                                        SWS_BICUBIC, NULL, NULL, NULL);


    int readPackCount = -1;
    int frame_cnt = 0;
    clock_t startTime = clock();

    //7. 开始一帧一帧读取
    while ((readPackCount = av_read_frame(avFormatContext, packet) >= 0)) {
        LOGI(" read fame count is %d", readPackCount);

        if (packet->stream_index == videoIndex) {
            //8. send AVPacket
            int sendPacket = avcodec_send_packet(pCodecContext, packet);
            //return 0 on success, otherwise negative error code:
            if (sendPacket != 0) {
                LOGE("avodec send packet error %d", sendPacket);
                continue;
            }
            //9. receive frame
            // 0:  success, a frame was returned
            int receiveFrame = avcodec_receive_frame(pCodecContext, pFrame);

            if (receiveFrame != 0) {
                //如果接收到的fame不等于0，忽略这次receiver否则会出现绿屏帧
                LOGE("avcodec_receive_frame error %d", receiveFrame);
                continue;
            }
            //10. 格式转换
            sws_scale(img_convert_ctx, (const uint8_t *const *) pFrame->data, pFrame->linesize,
                      0, pCodecContext->height,
                      pFrameYUV->data, pFrameYUV->linesize);

            //11. 分别写入YUV数据
            int y_size = pCodecParameters->width * pCodecParameters->height;
            //YUV420p
            fwrite(pFrameYUV->data[0], 1, y_size, pYUVFile);//Y
            fwrite(pFrameYUV->data[1], 1, y_size / 4, pYUVFile);//U
            fwrite(pFrameYUV->data[2], 1, y_size / 4, pYUVFile);//V

            //输出I、P、B帧信息
            char pictypeStr[10] = {0};
            switch (pFrame->pict_type) {
                case AV_PICTURE_TYPE_I: {
                    sprintf(pictypeStr, "I");
                    break;
                }
                case AV_PICTURE_TYPE_P: {
                    sprintf(pictypeStr, "P");
                    break;
                }
                case AV_PICTURE_TYPE_B: {
                    sprintf(pictypeStr, "B");
                    break;
                }
            }
            LOGI("Frame index %5d. Tpye %s", frame_cnt, pictypeStr);
            frame_cnt++;
        }
        //释放packet
        av_packet_unref(packet);
    }

    LOGI("frame count is %d", frame_cnt);
    clock_t endTime = clock();

    //long类型用%ld输出
    LOGI("decode video use Time %ld", (endTime - startTime));


    //12.释放相关资源



    sws_freeContext(img_convert_ctx);

    fclose(pYUVFile);

    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrame);
    avcodec_close(pCodecContext);
    avformat_close_input(&avFormatContext);
    return 0;

}


extern "C"
JNIEXPORT jint JNICALL
Java_android_spport_mylibrary2_Demo_decodeAudio(JNIEnv *env, jobject thiz, jstring video_path,
                                                jstring pcm_path) {

    //申请avFormatContext空间，记得要释放
    AVFormatContext *pFormatContext = avformat_alloc_context();

    const char *url = env->GetStringUTFChars(video_path, 0);

    //1. 打开媒体文件
    int result = avformat_open_input(&pFormatContext, url, NULL, NULL);
    if (result != 0) {
        LOGE("open input error url =%s,result=%d", url, result);
        return -1;
    }
    //2.读取媒体文件信息，给avFormatContext赋值
    result = avformat_find_stream_info(pFormatContext, NULL);
    if (result < 0) {
        LOGE("open input avformat_find_stream_info,result=%d", result);
        return -1;
    }
    ////3. 匹配到音频流的index
    int audioIndex = -1;
    for (int i = 0; i < pFormatContext->nb_streams; ++i) {
        AVMediaType codecType = pFormatContext->streams[i]->codecpar->codec_type;
        if (AVMEDIA_TYPE_AUDIO == codecType) {
            audioIndex = i;
            break;
        }
    }
    if (audioIndex == -1) {
        LOGE("not find a audio stream");
        return -1;
    }

    AVCodecParameters *pCodecParameters = pFormatContext->streams[audioIndex]->codecpar;

    //4. 根据流信息的codec_id找到对应的解码器
    AVCodec *pCodec = avcodec_find_decoder(pCodecParameters->codec_id);

    if (pCodec == NULL) {
        LOGE("Couldn`t find Codec");
        return -1;
    }

    AVCodecContext *pCodecContext = pFormatContext->streams[audioIndex]->codec;

    //5.使用给定的AVCodec初始化AVCodecContext
    int openResult = avcodec_open2(pCodecContext, pCodec, NULL);
    if (openResult < 0) {
        LOGE("avcodec open2 result %d", openResult);
        return -1;
    }

    const char *pcmPathStr = env->GetStringUTFChars(pcm_path, NULL);

    //新建一个二进制文件，已存在的文件将内容清空，允许读写
    FILE *pcmFile = fopen(pcmPathStr, "wb+");
    if (pcmFile == NULL) {
        LOGE(" fopen outPut file error");
        return -1;
    }

    //6. 初始化输出文件、解码AVPacket和AVFrame结构体
    auto *packet = (AVPacket *) av_malloc(sizeof(AVPacket));

    AVFrame *pFrame = av_frame_alloc();

    //7. 申请重采样SwrContext上下文
    SwrContext *swrContext = swr_alloc();

    int numBytes = 0;
    uint8_t *outData[2] = {0};
    int dstNbSamples = 0;                           // 解码目标的采样率

    int outChannel = 2;                             // 重采样后输出的通道
    //带P和不带P，关系到了AVFrame中的data的数据排列，不带P，则是LRLRLRLRLR排列，带P则是LLLLLRRRRR排列，
    // 若是双通道则带P则意味着data[0]全是L，data[1]全是R（注意：这是采样点不是字节），PCM播放器播放的文件需要的是LRLRLRLR的。
    //P表示Planar（平面），其数据格式排列方式为 (特别记住，该处是以点nb_samples采样点来交错，不是以字节交错）:
    //                    LLLLLLRRRRRRLLLLLLRRRRRRLLLLLLRRRRRRL...（每个LLLLLLRRRRRR为一个音频帧）
    //                    而不带P的数据格式（即交错排列）排列方式为：
    //                    LRLRLRLRLRLRLRLRLRLRLRLRLRLRLRLRLRLRL...（每个LR为一个音频样本）

    AVSampleFormat outFormat = AV_SAMPLE_FMT_S16P;  // 重采样后输出的格式
    int outSampleRate = 44100;                          // 重采样后输出的采样率

    // 通道布局与通道数据的枚举值是不同的，需要av_get_default_channel_layout转换
    swrContext = swr_alloc_set_opts(0,                                 // 输入为空，则会分配
                                    av_get_default_channel_layout(outChannel),
                                    outFormat,                         // 输出的采样频率
                                    outSampleRate,                     // 输出的格式
                                    av_get_default_channel_layout(pCodecContext->channels),
                                    pCodecContext->sample_fmt,       // 输入的格式
                                    pCodecContext->sample_rate,      // 输入的采样率
                                    0,
                                    0);

    //重采样初始化
    int swrInit = swr_init(swrContext);
    if (swrInit < 0) {
        LOGE("swr init error swrInit=%d", swrInit);
        return -1;
    }

    int frame_cnt = 0;

    outData[0] = (uint8_t *) av_malloc(1152 * 8);
    outData[1] = (uint8_t *) av_malloc(1152 * 8);

    //8. 开始一帧一帧读取
    while (av_read_frame(pFormatContext, packet) >= 0) {
        if (packet->stream_index == audioIndex) {
            //9。将封装包发往解码器
            int ret = avcodec_send_packet(pCodecContext, packet);
            if (ret) {
                LOGE("Failed to avcodec_send_packet(pAVCodecContext, pAVPacket) ,ret =%d", ret);
                break;
            }
//            LOGI("av_read_frame");
            // 10. 从解码器循环拿取数据帧
            while (!avcodec_receive_frame(pCodecContext, pFrame)) {
                // nb_samples并不是每个包都相同，遇见过第一个包为47，第二个包开始为1152的

                // 获取每个采样点的字节大小
                numBytes = av_get_bytes_per_sample(outFormat);
                //修改采样率参数后，需要重新获取采样点的样本个数
                dstNbSamples = av_rescale_rnd(pFrame->nb_samples,
                                              outSampleRate,
                                              pCodecContext->sample_rate,
                                              AV_ROUND_ZERO);
                // 重采样
                swr_convert(swrContext,
                            outData,
                            dstNbSamples,
                            (const uint8_t **) pFrame->data,
                            pFrame->nb_samples);
                LOGI("avcodec_receive_frame");
                // 第一次显示
                static bool show = true;
                if (show) {
                    LOGE("numBytes pFrame->nb_samples=%d dstNbSamples=%d,numBytes=%d,pCodecContext->sample_rate=%d,outSampleRate=%d", pFrame->nb_samples,
                         dstNbSamples,numBytes,pCodecContext->sample_rate,outSampleRate);
                    show = false;
                }
                // 使用LRLRLRLRLRL（采样点为单位，采样点有几个字节，交替存储到文件，可使用pcm播放器播放）
                for (int index = 0; index < dstNbSamples; index++) {
                    // // 交错的方式写入, 大部分float的格式输出 符合LRLRLRLR点交错模式
                    for (int channel = 0;channel < pCodecContext->channels; channel++)
                    {
                        fwrite((char *) outData[channel] + numBytes * index, 1, numBytes, pcmFile);
                    }
                }
                av_packet_unref(packet);
            }
            frame_cnt++;
        }
    }

    LOGI("frame count is %d", frame_cnt);

    swr_free(&swrContext);
    avcodec_close(pCodecContext);
    avformat_close_input(&pFormatContext);

    env->ReleaseStringUTFChars(video_path, url);

    env->ReleaseStringUTFChars(pcm_path, pcmPathStr);

    return 0;
}


void custom_log(void *ptr, int level, const char *fmt, va_list vl) {
    FILE *fp = fopen("/storage/emulated/0/av_log.txt", "a+");
    if (fp) {
        vfprintf(fp, fmt, vl);
        fflush(fp);
        fclose(fp);
    }
}

/**
 * 下面是雷神的版本
 * 使用的api是3.x的，不过基本的流程是一致的
 */
extern "C"
JNIEXPORT jint JNICALL
Java_android_spport_mylibrary2_Demo_decodeVideo2
        (JNIEnv *env, jobject obj, jstring input_jstr, jstring output_jstr) {
    AVFormatContext *pFormatCtx;
    int i, videoindex;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVFrame *pFrame, *pFrameYUV;
    uint8_t *out_buffer;
    AVPacket *packet;
    int y_size;
    int ret, got_picture;
    struct SwsContext *img_convert_ctx;
    FILE *fp_yuv;
    int frame_cnt;
    clock_t time_start, time_finish;
    long time_duration = 0;

    char input_str[500] = {0};
    char output_str[500] = {0};
    char info[1000] = {0};
    sprintf(input_str, "%s", env->GetStringUTFChars(input_jstr, NULL));
    sprintf(output_str, "%s", env->GetStringUTFChars(output_jstr, NULL));

    //FFmpeg av_log() callback
    av_log_set_callback(custom_log);

    av_register_all();
    avformat_network_init();
    pFormatCtx = avformat_alloc_context();

    if (avformat_open_input(&pFormatCtx, input_str, NULL, NULL) != 0) {
        LOGE("decodeVideo2-Couldn't open input stream.\n");
        return -1;
    }
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOGE("decodeVideo2-Couldn't find stream information.\n");
        return -1;
    }
    videoindex = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++)
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoindex = i;
            break;
        }
    if (videoindex == -1) {
        LOGE("decodeVideo2-Couldn't find a video stream.\n");
        return -1;
    }
    pCodecCtx = pFormatCtx->streams[videoindex]->codec;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL) {
        LOGE("decodeVideo2-Couldn't find Codec.\n");
        return -1;
    }
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOGE("decodeVideo2-Couldn't open codec.\n");
        return -1;
    }

    pFrame = av_frame_alloc();
    pFrameYUV = av_frame_alloc();
    out_buffer = (unsigned char *) av_malloc(
            av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1));
    av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer,
                         AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);

    packet = (AVPacket *) av_malloc(sizeof(AVPacket));

    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                                     pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P,
                                     SWS_BICUBIC, NULL, NULL, NULL);


    sprintf(info, "[Input     ]%s\n", input_str);
    sprintf(info, "%s[Output    ]%s\n", info, output_str);
    sprintf(info, "%s[Format    ]%s\n", info, pFormatCtx->iformat->name);
    sprintf(info, "%s[Codec     ]%s\n", info, pCodecCtx->codec->name);
    sprintf(info, "%s[Resolution]%dx%d\n", info, pCodecCtx->width, pCodecCtx->height);


    fp_yuv = fopen(output_str, "wb+");
    if (fp_yuv == NULL) {
        printf("decodeVideo2-Cannot open output file.\n");
        return -1;
    }

    frame_cnt = 0;
    time_start = clock();

    while (av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index == videoindex) {
            ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
            if (ret < 0) {
                LOGE("decodeVideo2-Decode Error.\n");
                return -1;
            }
            if (got_picture) {
                sws_scale(img_convert_ctx, (const uint8_t *const *) pFrame->data, pFrame->linesize,
                          0, pCodecCtx->height,
                          pFrameYUV->data, pFrameYUV->linesize);

                y_size = pCodecCtx->width * pCodecCtx->height;
                fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);    //Y
                fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
                fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V
                //Output info
                char pictype_str[10] = {0};
                switch (pFrame->pict_type) {
                    case AV_PICTURE_TYPE_I:
                        sprintf(pictype_str, "I");
                        break;
                    case AV_PICTURE_TYPE_P:
                        sprintf(pictype_str, "P");
                        break;
                    case AV_PICTURE_TYPE_B:
                        sprintf(pictype_str, "B");
                        break;
                    default:
                        sprintf(pictype_str, "Other");
                        break;
                }
                LOGI("decodeVideo2-Frame Index: %5d. Type:%s", frame_cnt, pictype_str);
                frame_cnt++;
            }
        }
        av_free_packet(packet);
    }
    //flush decoder
    //FIX: Flush Frames remained in Codec
//    while (1) {
//        ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
//        if (ret < 0)
//            break;
//        if (!got_picture)
//            break;
//        sws_scale(img_convert_ctx, (const uint8_t *const *) pFrame->data, pFrame->linesize, 0,
//                  pCodecCtx->height,
//                  pFrameYUV->data, pFrameYUV->linesize);
//        int y_size = pCodecCtx->width * pCodecCtx->height;
//        fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);    //Y
//        fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
//        fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V
//        //Output info
//        char pictype_str[10] = {0};
//        switch (pFrame->pict_type) {
//            case AV_PICTURE_TYPE_I:
//                sprintf(pictype_str, "I");
//                break;
//            case AV_PICTURE_TYPE_P:
//                sprintf(pictype_str, "P");
//                break;
//            case AV_PICTURE_TYPE_B:
//                sprintf(pictype_str, "B");
//                break;
//            default:
//                sprintf(pictype_str, "Other");
//                break;
//        }
//        LOGI("Frame Index: %5d. Type:%s", frame_cnt, pictype_str);
//        frame_cnt++;
//    }
    time_finish = clock();
    time_duration = (long) (time_finish - time_start);

    sprintf(info, "%s[Time      ]%fms\n", info, time_duration);
    sprintf(info, "%s[Count     ]%d\n", info, frame_cnt);

    LOGI("decodeVideo2-frame count is %d", frame_cnt);

    //long类型用%ld输出
    LOGI("decodeVideo2-decode video use Time %ld", time_duration);

    sws_freeContext(img_convert_ctx);

    fclose(fp_yuv);

    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrame);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);

    return 0;
}

