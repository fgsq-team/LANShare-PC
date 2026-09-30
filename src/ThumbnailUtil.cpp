#include "ThumbnailUtil.h"
#include <QDebug>

/*extern "C" {
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
}*/

QStringList imageExtensions = {"jpg", "jpeg", "png", "bmp", "gif"};
QStringList videoExtensions = {"mp4", "avi", "mov", "mkv"};

bool isVideoFile(const QString &fileName) {
    QString fileSuffix = fileName.mid(fileName.lastIndexOf('.') + 1).toLower();
    return videoExtensions.contains(fileSuffix);
}

bool isImageFile(const QString &fileName) {
    QString fileSuffix = fileName.mid(fileName.lastIndexOf('.') + 1).toLower();
    return imageExtensions.contains(fileSuffix);
}

QImage ThumbnailUtil::getThumnail(const QString &filePath, int width, int height) {
    QImage image;
    /*if (isVideoFile(filePath)) {
        // 加载图片资源
       av_register_all();
        AVFormatContext *fmtContext = nullptr;
        if (avformat_open_input(&fmtContext, filePath.toStdString().c_str(), nullptr, nullptr) < 0) {
            return image;
        }
        if (avformat_find_stream_info(fmtContext, nullptr) < 0) {
            avformat_close_input(&fmtContext);
            return image;
        }
        int nStreamIndex = -1;
        AVCodecParameters *codecParameters = nullptr;
        for (int i = 0; i < fmtContext->nb_streams; i++) {
            if (fmtContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                nStreamIndex = i;
                codecParameters = fmtContext->streams[i]->codecpar;
                break;
            }
        }
        if (nStreamIndex == -1) {
            avformat_close_input(&fmtContext);
            return image;
        }
        AVCodec *codec = avcodec_find_decoder(codecParameters->codec_id);
        if (!codec) {
            avformat_close_input(&fmtContext);
            return image;
        }
        AVCodecContext *codecContext = avcodec_alloc_context3(codec);
        if (!codecContext) {
            // 分配解码器上下文失败
            avformat_close_input(&fmtContext);
            return image;
        }
        if (avcodec_parameters_to_context(codecContext, codecParameters) < 0) {
            // 复制解码器参数到解码器上下文失败
            avcodec_free_context(&codecContext);
            avformat_close_input(&fmtContext);
            return image;
        }
        if (avcodec_open2(codecContext, codec, nullptr) < 0) {
            // 打开解码器失败
            avcodec_free_context(&codecContext);
            avformat_close_input(&fmtContext);
            return image;
        }
        AVPacket packet;
        av_init_packet(&packet);
        packet.data = nullptr;
        packet.size = 0;
        while (av_read_frame(fmtContext, &packet) >= 0) {
            if (packet.stream_index == nStreamIndex) {
                AVFrame *frame = av_frame_alloc();
                if (frame) {
                    int ret = avcodec_send_packet(codecContext, &packet);
                    if (ret >= 0) {
                        ret = avcodec_receive_frame(codecContext, frame);
                        if (ret >= 0) {
                            // 将第一帧保存为封面图像
                            if (frame->key_frame) {
                                AVFrame *rgbFrame = av_frame_alloc();
                                if (rgbFrame) {
                                    rgbFrame->format = AV_PIX_FMT_RGB24;
                                    rgbFrame->width = frame->width;
                                    rgbFrame->height = frame->height;
                                    int bufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, frame->width,
                                                                              frame->height,
                                                                              1);
                                    auto *buffer = new uint8_t[bufferSize];
                                    av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, buffer, AV_PIX_FMT_RGB24,
                                                         frame->width, frame->height, 1);
                                    SwsContext *swsContext = sws_getContext(frame->width, frame->height,
                                                                            codecContext->pix_fmt,
                                                                            frame->width, frame->height,
                                                                            AV_PIX_FMT_RGB24,
                                                                            SWS_BICUBIC, nullptr, nullptr, nullptr);
                                    if (swsContext) {
                                        sws_scale(swsContext, frame->data, frame->linesize, 0, frame->height,
                                                  rgbFrame->data, rgbFrame->linesize);
                                        sws_freeContext(swsContext);
                                        // 保存封面图像到文件
                                        int outputBufferSize = rgbFrame->width * rgbFrame->height * 3;
                                        auto *outputBuffer = new uchar[outputBufferSize];
                                        for (int i = 0; i < rgbFrame->height; i++) {
                                            memcpy(outputBuffer + i * rgbFrame->width * 3,
                                                   rgbFrame->data[0] + i * rgbFrame->linesize[0], rgbFrame->width * 3);
                                        }
                                        image = QImage(outputBuffer, rgbFrame->width, rgbFrame->height,
                                                       QImage::Format_RGB888).copy();
                                        if (width != -1 && height != -1) {
                                            image = image.scaled(width, height, Qt::KeepAspectRatio,
                                                                 Qt::SmoothTransformation);
                                        }
                                        if (outputBuffer) {
                                            delete[] outputBuffer;
                                            outputBuffer = nullptr;
                                        }
                                    }
                                    if (buffer) {
                                        delete[] buffer;
                                        buffer = nullptr;
                                    }
                                    av_frame_free(&rgbFrame);
                                }
                            }
                        }
                    }

                    av_frame_free(&frame);
                }
                break;
            }
            av_packet_unref(&packet);
        }
        avcodec_free_context(&codecContext);
        avformat_close_input(&fmtContext);
        return image;
    } else*/ if (isImageFile(filePath)) {
        image = QImage(filePath);
        if (width != -1 && height != -1) {
            image = image.scaled(width, height, Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation);
        }
        return image;
    }
    return {};
}
