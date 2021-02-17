//
// Created by ms on 2021/2/17.
//

#include "Transcode.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>
#include <libavutil/adler32.h>

typedef struct _State {
    AVFormatContext *fmt_ctx;
    AVCodec *video_codec;
    AVCodec *audio_codec;
    AVCodecContext *video_ctx;
    AVCodecContext *audio_ctx;
    int video_stream;
    int audio_stream;
} State;

static int video_decode_example(const char *input_filename) {
    State *state = (State *) malloc(sizeof(State));
    memset(state, 0, sizeof(State));

    AVFrame *fr = NULL;
    uint8_t *byte_buffer = NULL;
    AVPacket pkt;
    int number_of_written_bytes;
    int got_frame = 0;
    int byte_buffer_size;
    int i = 0;
    int result;
    int end_of_stream = 0;

    result = avformat_open_input(&state->fmt_ctx, input_filename, NULL, NULL);
    if (result < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open file\n");
        return result;
    }

    result = avformat_find_stream_info(state->fmt_ctx, NULL);
    if (result < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't get stream info\n");
        return result;
    }


    for (int i = 0; i < state->fmt_ctx->nb_streams; i++) {
        if (state->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            state->video_stream = av_find_best_stream(state->fmt_ctx,
                                                      state->fmt_ctx->streams[state->video_stream]->codecpar->codec_type,
                                                      -1,
                                                      -1, NULL, 0);
            if (state->video_stream < 0) {
                av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
                return -1;
            }

            AVCodecParameters *origin_par = state->fmt_ctx->streams[state->video_stream]->codecpar;

            state->video_codec = avcodec_find_decoder(origin_par->codec_id);
            if (!state->video_codec) {
                av_log(NULL, AV_LOG_ERROR, "Can't find decoder\n");
                return -1;
            }

            state->video_ctx = avcodec_alloc_context3(state->video_codec);
            if (!state->video_ctx) {
                av_log(NULL, AV_LOG_ERROR, "Can't allocate decoder context\n");
                return AVERROR(ENOMEM);
            }

            result = avcodec_parameters_to_context(state->video_ctx, origin_par);
            if (result) {
                av_log(NULL, AV_LOG_ERROR, "Can't copy decoder context\n");
                return result;
            }
        } else if (state->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            state->audio_stream = av_find_best_stream(state->fmt_ctx,
                                                      state->fmt_ctx->streams[state->audio_stream]->codecpar->codec_type,
                                                      -1,
                                                      -1, NULL, 0);
            if (state->audio_stream < 0) {
                av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
                return -1;
            }

            AVCodecParameters *origin_par = state->fmt_ctx->streams[state->audio_stream]->codecpar;

            state->audio_codec = avcodec_find_decoder(origin_par->codec_id);
            if (!state->audio_codec) {
                av_log(NULL, AV_LOG_ERROR, "Can't find decoder\n");
                return -1;
            }

            state->audio_ctx = avcodec_alloc_context3(state->audio_codec);
            if (!state->audio_ctx) {
                av_log(NULL, AV_LOG_ERROR, "Can't allocate decoder context\n");
                return AVERROR(ENOMEM);
            }

            result = avcodec_parameters_to_context(state->audio_ctx, origin_par);
            if (result) {
                av_log(NULL, AV_LOG_ERROR, "Can't copy decoder context\n");
                return result;
            }
        }
    }


    result = avcodec_open2(state->video_ctx, state->video_codec, NULL);
    if (result < 0) {
        av_log(state->video_ctx, AV_LOG_ERROR, "Can't open decoder\n");
        return result;
    }

    fr = av_frame_alloc();
    if (!fr) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate frame\n");
        return AVERROR(ENOMEM);
    }

    byte_buffer_size = av_image_get_buffer_size(state->video_ctx->pix_fmt, state->video_ctx->width,
                                                state->video_ctx->height, 16);
    byte_buffer = static_cast<uint8_t *>(av_malloc(byte_buffer_size));
    if (!byte_buffer) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate buffer\n");
        return AVERROR(ENOMEM);
    }

    printf("#tb %d: %d/%d\n", state->video_stream, state->fmt_ctx->streams[state->video_stream]->time_base.num,
           state->fmt_ctx->streams[state->video_stream]->time_base.den);
    i = 0;
    av_init_packet(&pkt);
    do {
        if (!end_of_stream)
            if (av_read_frame(state->fmt_ctx, &pkt) < 0)
                end_of_stream = 1;
        if (end_of_stream) {
            pkt.data = NULL;
            pkt.size = 0;
        }
        if (pkt.stream_index == state->video_stream || end_of_stream) {
            got_frame = 0;
            if (pkt.pts == AV_NOPTS_VALUE)
                pkt.pts = pkt.dts = i;
            result = avcodec_decode_video2(state->video_ctx, fr, &got_frame, &pkt);
            if (result < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error decoding frame\n");
                return result;
            }
            if (got_frame) {
                number_of_written_bytes = av_image_copy_to_buffer(byte_buffer, byte_buffer_size,
                                                                  (const uint8_t *const *) fr->data,
                                                                  (const int *) fr->linesize,
                                                                  state->video_ctx->pix_fmt, state->video_ctx->width,
                                                                  state->video_ctx->height, 1);
                if (number_of_written_bytes < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Can't copy image to buffer\n");
                    return number_of_written_bytes;
                }
                printf("%d, %s, %s, %8" PRId64", %8d, 0x%08lx\n", state->video_stream,
                       av_ts2str(fr->pts), av_ts2str(fr->pkt_dts), fr->pkt_duration,
                       number_of_written_bytes,
                       av_adler32_update(0, (const uint8_t *) byte_buffer, number_of_written_bytes));
            }
            av_packet_unref(&pkt);
            av_init_packet(&pkt);
        }
        i++;
    } while (!end_of_stream || got_frame);

    av_packet_unref(&pkt);
    av_frame_free(&fr);
    avformat_close_input(&state->fmt_ctx);
    avcodec_free_context(&state->video_ctx);
    av_freep(&byte_buffer);
    return 0;
}


int Transcode::trans(const char *srcPath, const char *dstPath) {

}