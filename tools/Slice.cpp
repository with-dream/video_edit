//
// Created by ms on 2021/2/16.
//
//https://blog.csdn.net/weixin_33757609/article/details/88010116
#include "Slice.h"

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#include "libavcodec/avcodec.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/audio_fifo.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libavutil/timestamp.h"
#include "libavutil/adler32.h"

#ifdef __cplusplus
};
#endif

int Slice::slice(const char *srcPath, float start, float end, const char *dstPath) {
    AVFormatContext *fmt_ctx = nullptr;
    AVFormatContext *ofmt_ctx = nullptr;
    AVCodec *vcodec = nullptr, *acodec = nullptr;
    AVCodecContext *vCodecCtx = nullptr, *aCodecCtx = nullptr;
    int result;

    result = avformat_open_input(&fmt_ctx, srcPath, nullptr, nullptr);
    if (result < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Can't open file\n");
        return result;
    }

    result = avformat_find_stream_info(fmt_ctx, nullptr);
    if (result < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Can't get stream info\n");
        return result;
    }

    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, dstPath);

    for (int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            AVCodecParameters *origin_par = fmt_ctx->streams[i]->codecpar;

            vcodec = avcodec_find_decoder(origin_par->codec_id);
            if (!vcodec) {
                av_log(nullptr, AV_LOG_ERROR, "Can't find decoder\n");
                return -1;
            }

            vCodecCtx = avcodec_alloc_context3(vcodec);
            if (!vCodecCtx) {
                av_log(nullptr, AV_LOG_ERROR, "Can't allocate decoder context\n");
                return AVERROR(ENOMEM);
            }

            result = avcodec_parameters_to_context(vCodecCtx, origin_par);
            if (result) {
                av_log(nullptr, AV_LOG_ERROR, "Can't copy decoder context\n");
                return result;
            }

            AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
            if (!out_stream) {
                fprintf(stderr, "Failed allocating output stream\n");
                result = AVERROR_UNKNOWN;
                return -1;
            }
            avcodec_parameters_copy(out_stream->codecpar, origin_par);
            out_stream->codecpar->codec_tag = 0;
        } else if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            AVCodecParameters *origin_par = fmt_ctx->streams[i]->codecpar;

            acodec = avcodec_find_decoder(origin_par->codec_id);
            if (!acodec) {
                av_log(nullptr, AV_LOG_ERROR, "Can't find decoder\n");
                return -1;
            }

            aCodecCtx = avcodec_alloc_context3(acodec);
            if (!aCodecCtx) {
                av_log(nullptr, AV_LOG_ERROR, "Can't allocate decoder context\n");
                return AVERROR(ENOMEM);
            }

            result = avcodec_parameters_to_context(aCodecCtx, origin_par);
            if (result) {
                av_log(nullptr, AV_LOG_ERROR, "Can't copy decoder context\n");
                return result;
            }

            AVStream *out_stream = avformat_new_stream(ofmt_ctx, NULL);
            if (!out_stream) {
                fprintf(stderr, "Failed allocating output stream\n");
                result = AVERROR_UNKNOWN;
                return -1;
            }
            avcodec_parameters_copy(out_stream->codecpar, origin_par);
            out_stream->codecpar->codec_tag = 0;
        }
    }

    avio_open(&ofmt_ctx->pb, dstPath, AVIO_FLAG_WRITE);
    result = avformat_write_header(ofmt_ctx, NULL);
    if (result < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        return -1;
    }

    result = av_seek_frame(fmt_ctx, -1, start * AV_TIME_BASE, AVSEEK_FLAG_BACKWARD);
    if (result < 0) {
        fprintf(stderr, "Error seek\n");
        return -1;
    }

    int64_t *dts_start_from = static_cast<int64_t *>(malloc(sizeof(int64_t) * fmt_ctx->nb_streams));
    memset(dts_start_from, 0, sizeof(int64_t) * fmt_ctx->nb_streams);

    int64_t *pts_start_from = static_cast<int64_t *>(malloc(sizeof(int64_t) * fmt_ctx->nb_streams));
    memset(pts_start_from, 0, sizeof(int64_t) * fmt_ctx->nb_streams);

    AVPacket pkt;
    while (1) {
        AVStream *in_stream, *out_stream;

        //读取数据
        result = av_read_frame(fmt_ctx, &pkt);
        if (result < 0) {
            if (result == AVERROR_EOF)
                fprintf(stderr, "文件尾\n");
            break;
        }


        in_stream = fmt_ctx->streams[pkt.stream_index];
        out_stream = ofmt_ctx->streams[pkt.stream_index];

        // 时间超过要截取的时间，就退出循环
        if (av_q2d(in_stream->time_base) * pkt.pts > end) {
            av_packet_unref(&pkt);
            break;
        }

        // 将截取后的每个流的起始dts 、pts保存下来，作为开始时间，用来做后面的时间基转换
        if (dts_start_from[pkt.stream_index] == 0) {
            dts_start_from[pkt.stream_index] = pkt.dts;
        }
        if (pts_start_from[pkt.stream_index] == 0) {
            pts_start_from[pkt.stream_index] = pkt.pts;
        }

        // 时间基转换
        pkt.pts = av_rescale_q_rnd(pkt.pts - pts_start_from[pkt.stream_index], in_stream->time_base,
                                   out_stream->time_base,
                                   static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts - dts_start_from[pkt.stream_index], in_stream->time_base,
                                   out_stream->time_base,
                                   static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

        if (pkt.pts < 0) {
            pkt.pts = 0;
        }
        if (pkt.dts < 0) {
            pkt.dts = 0;
        }

        pkt.duration = (int) av_rescale_q((int64_t) pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;

        //一帧视频播放时间必须在解码时间点之后，当出现pkt.pts < pkt.dts时会导致程序异常，所以我们丢掉有问题的帧，不会有太大影响。
        if (pkt.pts < pkt.dts) {
            continue;
        }

        result = av_interleaved_write_frame(ofmt_ctx, &pkt);
        if (result < 0) {
            fprintf(stderr, "Error write packet\n");
            break;
        }

        av_packet_unref(&pkt);
    }

//释放资源
    free(dts_start_from);
    free(pts_start_from);

//写文件尾信息
    av_write_trailer(ofmt_ctx);


    avformat_close_input(&fmt_ctx);
    avcodec_free_context(&vCodecCtx);
    avcodec_free_context(&aCodecCtx);

    return 0;
}