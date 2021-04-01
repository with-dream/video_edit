////
//// Created by ms on 2021/2/17.
////
//
//#include "Transcode.h"
//#include <stdlib.h>
//#include <stdio.h>
//#include <string.h>
//
//static int stream_component_open(VideoState *is, int stream_index) {
//    AVFormatContext *ic = is->ic;
//    AVCodecContext *avctx;
//    const AVCodec *codec;
//    const char *forced_codec_name = nullptr;
//    int sample_rate, nb_channels;
//    int64_t channel_layout;
//    int ret = 0;
//
//    if (stream_index < 0 || stream_index >= ic->nb_streams)
//        return -1;
//
//    avctx = avcodec_alloc_context3(nullptr);
//    if (!avctx)
//        return AVERROR(ENOMEM);
//
//    ret = avcodec_parameters_to_context(avctx, ic->streams[stream_index]->codecpar);
//    if (ret < 0)
//        goto fail;
//    avctx->pkt_timebase = ic->streams[stream_index]->time_base;
//
//    codec = avcodec_find_decoder(avctx->codec_id);
//
//    if (forced_codec_name)
//        codec = avcodec_find_decoder_by_name(forced_codec_name);
//    if (!codec) {
//        if (forced_codec_name)
//            av_log(NULL, AV_LOG_WARNING,
//                   "No codec could be found with name '%s'\n", forced_codec_name);
//        else
//            av_log(NULL, AV_LOG_WARNING,
//                   "No decoder could be found for codec %s\n", avcodec_get_name(avctx->codec_id));
//        ret = AVERROR(EINVAL);
//        goto fail;
//    }
//
//    avctx->codec_id = codec->id;
//
//    if ((ret = avcodec_open2(avctx, codec, nullptr)) < 0) {
//        goto fail;;
//    }
//
//    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
//    switch (avctx->codec_type) {
//        case AVMEDIA_TYPE_AUDIO:
//#if CONFIG_AVFILTER
//            {
//            AVFilterContext *sink;
//
//            is->audio_filter_src.freq           = avctx->sample_rate;
//            is->audio_filter_src.channels       = avctx->channels;
//            is->audio_filter_src.channel_layout = get_valid_channel_layout(avctx->channel_layout, avctx->channels);
//            is->audio_filter_src.fmt            = avctx->sample_fmt;
//            if ((ret = configure_audio_filters(is, afilters, 0)) < 0)
//                goto fail;
//            sink = is->out_audio_filter;
//            sample_rate    = av_buffersink_get_sample_rate(sink);
//            nb_channels    = av_buffersink_get_channels(sink);
//            channel_layout = av_buffersink_get_channel_layout(sink);
//        }
//#else
//            sample_rate = avctx->sample_rate;
//            nb_channels = avctx->channels;
//            channel_layout = avctx->channel_layout;
//#endif
//
//            if ((ret = decoder_init(&is->auddec, avctx, &is->audioq, is->continue_read_thread)) < 0)
//                goto fail;
//            if ((is->ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) &&
//                !is->ic->iformat->read_seek) {
//                is->auddec.start_pts = is->audio_st->start_time;
//                is->auddec.start_pts_tb = is->audio_st->time_base;
//            }
//            if ((ret = decoder_start(&is->auddec, audio_thread, "audio_decoder", is)) < 0)
//                goto out;
//            break;
//        case AVMEDIA_TYPE_VIDEO:
//            is->video_stream = stream_index;
//            is->video_st = ic->streams[stream_index];
//
//            if ((ret = decoder_init(&is->viddec, avctx, &is->videoq, is->continue_read_thread)) < 0)
//                goto fail;
//            if ((ret = decoder_start(&is->viddec, video_thread, "video_decoder", is)) < 0)
//                goto out;
//            is->queue_attachments_req = 1;
//            break;
//
//        default:
//            break;
//    }
//
//    fail:
//    avcodec_free_context(&avctx);
//    return ret;
//}
//
//int Transcode::init(const char *srcPath, const char *dstPath) {
//    bool video_disable = false, audio_disable = false;
//    is = (VideoState *) malloc(sizeof(VideoState));
//    memset(is, 0, sizeof(VideoState));
//
//    is->ic = avformat_alloc_context();
//    if (!is->ic) {
//        av_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
//        return AVERROR(ENOMEM);
//    }
//
//    int result;
//    result = avformat_open_input(&is->ic, srcPath, NULL, NULL);
//    if (result < 0) {
//        av_log(NULL, AV_LOG_ERROR, "Can't open file\n");
//        return result;
//    }
//
//    result = avformat_find_stream_info(is->ic, NULL);
//    if (result < 0) {
//        av_log(NULL, AV_LOG_ERROR, "Can't get stream info\n");
//        return result;
//    }
//
//    if (!video_disable)
//        is->video_stream = av_find_best_stream(is->ic,
//                                               AVMEDIA_TYPE_VIDEO,
//                                               -1,
//                                               -1, nullptr, 0);
//    if (!audio_disable)
//        is->video_stream = av_find_best_stream(is->ic,
//                                               AVMEDIA_TYPE_AUDIO,
//                                               -1,
//                                               -1, nullptr, 0);
//    if (is->audio_stream >= 0)
//        stream_component_open(is, is->audio_stream);
//    if (is->video_stream >= 0)
//        stream_component_open(is, is->video_stream);
////
////    for (int i = 0; i < is->fmt_ctx->nb_streams; i++) {
////        if (is->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
////            is->video_stream = av_find_best_stream(state->fmt_ctx,
////                                                      state->fmt_ctx->streams[state->video_stream]->codecpar->codec_type,
////                                                      -1,
////                                                      -1, NULL, 0);
////            if (is->video_stream < 0) {
////                av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
////                return -1;
////            }
////
////            AVCodecParameters *origin_par = state->fmt_ctx->streams[state->video_stream]->codecpar;
////
////            is->video_codec = avcodec_find_decoder(origin_par->codec_id);
////            if (!is->video_codec) {
////                av_log(NULL, AV_LOG_ERROR, "Can't find decoder\n");
////                return -1;
////            }
////
////            is->video_ctx = avcodec_alloc_context3(is->video_codec);
////            if (!is->video_ctx) {
////                av_log(NULL, AV_LOG_ERROR, "Can't allocate decoder context\n");
////                return AVERROR(ENOMEM);
////            }
////
////            result = avcodec_parameters_to_context(is->video_ctx, origin_par);
////            if (result) {
////                av_log(NULL, AV_LOG_ERROR, "Can't copy decoder context\n");
////                return result;
////            }
////        } else if (state->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
////            state->audio_stream = av_find_best_stream(state->fmt_ctx,
////                                                      state->fmt_ctx->streams[state->audio_stream]->codecpar->codec_type,
////                                                      -1,
////                                                      -1, NULL, 0);
////            if (state->audio_stream < 0) {
////                av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
////                return -1;
////            }
////
////            AVCodecParameters *origin_par = state->fmt_ctx->streams[state->audio_stream]->codecpar;
////
////            state->audio_codec = avcodec_find_decoder(origin_par->codec_id);
////            if (!state->audio_codec) {
////                av_log(NULL, AV_LOG_ERROR, "Can't find decoder\n");
////                return -1;
////            }
////
////            state->audio_ctx = avcodec_alloc_context3(state->audio_codec);
////            if (!state->audio_ctx) {
////                av_log(NULL, AV_LOG_ERROR, "Can't allocate decoder context\n");
////                return AVERROR(ENOMEM);
////            }
////
////            result = avcodec_parameters_to_context(state->audio_ctx, origin_par);
////            if (result) {
////                av_log(NULL, AV_LOG_ERROR, "Can't copy decoder context\n");
////                return result;
////            }
////        }
////    }
//
//}
//
//static int video_decode_example(const char *input_filename) {
//
//
//    result = avcodec_open2(state->video_ctx, state->video_codec, NULL);
//    if (result < 0) {
//        av_log(state->video_ctx, AV_LOG_ERROR, "Can't open decoder\n");
//        return result;
//    }
//
//    fr = av_frame_alloc();
//    if (!fr) {
//        av_log(NULL, AV_LOG_ERROR, "Can't allocate frame\n");
//        return AVERROR(ENOMEM);
//    }
//
//    byte_buffer_size = av_image_get_buffer_size(state->video_ctx->pix_fmt, state->video_ctx->width,
//                                                state->video_ctx->height, 16);
//    byte_buffer = static_cast<uint8_t *>(av_malloc(byte_buffer_size));
//    if (!byte_buffer) {
//        av_log(NULL, AV_LOG_ERROR, "Can't allocate buffer\n");
//        return AVERROR(ENOMEM);
//    }
//
//    printf("#tb %d: %d/%d\n", state->video_stream, state->fmt_ctx->streams[state->video_stream]->time_base.num,
//           state->fmt_ctx->streams[state->video_stream]->time_base.den);
//    i = 0;
//    av_init_packet(&pkt);
//    do {
//        if (!end_of_stream)
//            if (av_read_frame(state->fmt_ctx, &pkt) < 0)
//                end_of_stream = 1;
//        if (end_of_stream) {
//            pkt.data = NULL;
//            pkt.size = 0;
//        }
//        if (pkt.stream_index == state->video_stream || end_of_stream) {
//            got_frame = 0;
//            if (pkt.pts == AV_NOPTS_VALUE)
//                pkt.pts = pkt.dts = i;
//            result = avcodec_decode_video2(state->video_ctx, fr, &got_frame, &pkt);
//            if (result < 0) {
//                av_log(NULL, AV_LOG_ERROR, "Error decoding frame\n");
//                return result;
//            }
//            if (got_frame) {
//                number_of_written_bytes = av_image_copy_to_buffer(byte_buffer, byte_buffer_size,
//                                                                  (const uint8_t *const *) fr->data,
//                                                                  (const int *) fr->linesize,
//                                                                  state->video_ctx->pix_fmt, state->video_ctx->width,
//                                                                  state->video_ctx->height, 1);
//                if (number_of_written_bytes < 0) {
//                    av_log(NULL, AV_LOG_ERROR, "Can't copy image to buffer\n");
//                    return number_of_written_bytes;
//                }
//                printf("%d, %s, %s, %8" PRId64", %8d, 0x%08lx\n", state->video_stream,
//                       av_ts2str(fr->pts), av_ts2str(fr->pkt_dts), fr->pkt_duration,
//                       number_of_written_bytes,
//                       av_adler32_update(0, (const uint8_t *) byte_buffer, number_of_written_bytes));
//            }
//            av_packet_unref(&pkt);
//            av_init_packet(&pkt);
//        }
//        i++;
//    } while (!end_of_stream || got_frame);
//
//    av_packet_unref(&pkt);
//    av_frame_free(&fr);
//    avformat_close_input(&state->fmt_ctx);
//    avcodec_free_context(&state->video_ctx);
//    av_freep(&byte_buffer);
//    return 0;
//}
//
//
//int Transcode::trans() {
//
//}