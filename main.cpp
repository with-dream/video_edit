/*
 * Copyright (c) 2010 Nicolas George
 * Copyright (c) 2011 Stefano Sabatini
 * Copyright (c) 2014 Andrey Utkin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * API example for demuxing, decoding, filtering, encoding and muxing
 * @example transcoding.c
 */
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}

typedef struct _media_param {
    int width;
    int height;
    AVRational sample_aspect_ratio;
    enum AVPixelFormat pix_fmt;
    AVRational framerate;
    AVRational video_time_base;
    int sample_rate;
    int channel_layout;
    int channels;
    enum AVSampleFormat sample_fmt;
    AVRational audio_time_base;
} media_param;

typedef struct _media_info {
    char *file_name;
    long start;
    long end;
    AVFormatContext *ic;
    int stream_index[AVMEDIA_TYPE_NB];
    AVCodecContext *avctx[AVMEDIA_TYPE_NB];
    AVFrame *frame[AVMEDIA_TYPE_NB];
    AVPacket *packet[AVMEDIA_TYPE_NB];

    char *video_codec_name;
    char *audio_codec_name;
    char *subtitle_codec_name;

    int video_disable;
    int audio_disable;
    int subtitle_disable;
} media_info;

typedef struct _vecontext {
    media_info *input_media[1];
    media_info *output_media[1];
} vecontext;

typedef struct _TT {
    AVFrame *frame;
    int input_index;
    int output_index;
} TT;

static int init_avctx(media_info *info, AVStream *stream, media_param *param, bool decode) {
    char *force_codec = nullptr;
    int type = stream->codecpar->codec_type;
    switch (type) {
        case AVMEDIA_TYPE_VIDEO:
            force_codec = info->video_codec_name;
            break;
        case AVMEDIA_TYPE_AUDIO:
            force_codec = info->audio_codec_name;
            break;
        case AVMEDIA_TYPE_SUBTITLE:
            force_codec = info->subtitle_codec_name;
            break;
    }

    AVCodec *dec;
    av_log(NULL, AV_LOG_ERROR, "id:%d  index:%d\n", stream->codecpar->codec_id, stream->index);
    if (!force_codec && stream->codecpar->codec_id == AV_CODEC_ID_NONE) {
        stream->codecpar->codec_id = av_guess_codec(info->ic->oformat, nullptr, info->file_name,
                                                    nullptr, stream->codecpar->codec_type);
        dec = avcodec_find_encoder(stream->codecpar->codec_id);
    } else {
        if (decode)
            dec = force_codec ? avcodec_find_decoder_by_name(force_codec)
                              : avcodec_find_decoder(stream->codecpar->codec_id);
        else
            dec = force_codec ? avcodec_find_encoder_by_name(force_codec)
                              : avcodec_find_encoder(stream->codecpar->codec_id);
    }
    if (!dec) {
        av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u\n", type);
        return AVERROR_DECODER_NOT_FOUND;
    }
    AVCodecContext *codec_ctx = avcodec_alloc_context3(dec);
    if (!codec_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate the coder context for stream #%u\n", type);
        return AVERROR(ENOMEM);
    }

    if (param) {
        switch (type) {
            case AVMEDIA_TYPE_VIDEO:
                codec_ctx->width = param->width;
                codec_ctx->height = param->height;
                codec_ctx->sample_aspect_ratio = param->sample_aspect_ratio;
                codec_ctx->pix_fmt = param->pix_fmt;
                codec_ctx->framerate = param->framerate;
                codec_ctx->time_base = param->video_time_base;
                stream->time_base = codec_ctx->time_base;
                break;
            case AVMEDIA_TYPE_AUDIO:
                codec_ctx->sample_rate = param->sample_rate;
                codec_ctx->channel_layout = param->channel_layout;
                codec_ctx->channels = param->channels;
                codec_ctx->sample_fmt = param->sample_fmt;
//                codec_ctx->time_base = param->audio_time_base;
//                stream->time_base = codec_ctx->time_base;
                break;
            case AVMEDIA_TYPE_SUBTITLE:
                codec_ctx->time_base = param->video_time_base;
                break;
        }
    }

    int ret = -1;
    ret = decode ? avcodec_parameters_to_context(codec_ctx, stream->codecpar)
                 : avcodec_parameters_from_context(stream->codecpar, codec_ctx);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to copy coder parameters to input coder context "
                                   "for stream #%u\n", type);
        return ret;
    }

    if (info->ic->oformat && info->ic->oformat->flags & AVFMT_GLOBALHEADER)
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
        codec_ctx->framerate = av_guess_frame_rate(info->ic, stream, nullptr);
    ret = avcodec_open2(codec_ctx, dec, nullptr);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to open coder for stream #%u\n", type);
        return ret;
    }

    info->avctx[stream->index] = codec_ctx;
    info->frame[stream->index] = av_frame_alloc();
//    info->stream_index[type] = stream->index;
    return 0;
}

static int open_input_file(media_info *info) {
    int ret;
    unsigned int i;

    if ((ret = avformat_open_input(&info->ic, info->file_name, nullptr, nullptr)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(info->ic, nullptr)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    for (i = 0; i < info->ic->nb_streams; i++) {
        AVStream *stream = info->ic->streams[i];
        if (init_avctx(info, stream, nullptr, true) < 0)
            return -1;
    }
    av_dump_format(info->ic, 0, info->file_name, 0);
    return 0;
}

static int open_output_file(media_info *info, media_param *param) {
    avformat_alloc_output_context2(&info->ic, nullptr, nullptr, info->file_name);
    if (!info->ic) {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }

    int ret = -1;
    if (!info->video_disable &&
        av_guess_codec(info->ic->oformat, nullptr, info->file_name, nullptr, AVMEDIA_TYPE_VIDEO) != AV_CODEC_ID_NONE) {
        AVStream *stream = avformat_new_stream(info->ic, nullptr);
        stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        if (init_avctx(info, stream, param, false) < 0)
            return -1;
        info->packet[stream->index] = av_packet_alloc();
    }
    if (!info->audio_disable &&
        av_guess_codec(info->ic->oformat, nullptr, info->file_name, nullptr, AVMEDIA_TYPE_AUDIO) != AV_CODEC_ID_NONE) {
        AVStream *stream = avformat_new_stream(info->ic, nullptr);
        stream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
        if (init_avctx(info, stream, param, false) < 0)
            return -1;
        info->packet[stream->index] = av_packet_alloc();
    }
    if (!info->subtitle_disable &&
        av_guess_codec(info->ic->oformat, nullptr, info->file_name, nullptr, AVMEDIA_TYPE_SUBTITLE) !=
        AV_CODEC_ID_NONE) {
        AVStream *stream = avformat_new_stream(info->ic, nullptr);
        stream->codecpar->codec_type = AVMEDIA_TYPE_SUBTITLE;
        if (init_avctx(info, stream, param, false) < 0)
            return -1;
        info->packet[stream->index] = av_packet_alloc();
    }
    av_dump_format(info->ic, 0, info->file_name, 1);

    if (!(info->ic->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&info->ic->pb, info->file_name, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", info->file_name);
            return ret;
        }
    }
    /* init muxer, write output file header */
    ret = avformat_write_header(info->ic, nullptr);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }

    return 0;
}

static int
encode_write_frame(media_info *out_info, media_info *in_info, TT *tt) {
    if (!out_info->avctx[tt->output_index])
        return 0;

    AVFrame *filt_frame = tt->frame;
    AVPacket *enc_pkt = av_packet_alloc();
    int ret;

    /* encode filtered frame */
//    double enP = 0;
//    if (filt_frame)
//        enP = filt_frame->pts * av_q2d(out_info->avctx[stream_index]->time_base);
//    av_log(NULL, AV_LOG_INFO, "Encoding frame ret:%f\n", enP);

//                           out_info->ic->streams[stream_index]->time_base);
    ret = avcodec_send_frame(out_info->avctx[tt->output_index], filt_frame);

    if (ret < 0)
        return ret;

    while (ret >= 0) {
        ret = avcodec_receive_packet(out_info->avctx[tt->output_index], enc_pkt);
//        av_log(NULL, AV_LOG_INFO, "receive packet ret:%d\n", ret);
        double enP = 0;
        if (filt_frame)
            enP = enc_pkt->pts * av_q2d(out_info->avctx[tt->output_index]->time_base);
        av_log(NULL, AV_LOG_INFO, "Encoding frame index:%d ret:%f\n", tt->output_index, enP);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;

        enc_pkt->stream_index = tt->output_index;
//        int64_t ps = av_rescale_q(enc_pkt->pts, out_info->avctx[stream_index]->time_base,
//                                  out_info->ic->streams[stream_index]->time_base);
//        av_log(NULL, AV_LOG_INFO, "Muxing frame  pkt:%ld\n", ps);
        /* mux encoded frame */
        ret = av_interleaved_write_frame(out_info->ic, enc_pkt);
    }
    av_packet_unref(enc_pkt);

    return ret;
}

static void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize,
                     char *filename) {
    FILE *f;
    int i;

    f = fopen(filename, "wb");
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}

int main(int argc, char **argv) {
    avformat_network_init();
    vecontext *ctx = (vecontext *) malloc(sizeof(vecontext));
    memset(ctx, 0, sizeof(vecontext));
//    media_info **tmp = (media_info **) malloc(sizeof(media_info *) * 1);
//    ctx->inputMedia = tmp;

    media_info *input_info = (media_info *) malloc(sizeof(media_info));
    memset(input_info, 0, sizeof(media_info));
    input_info->file_name = "/Users/ms/Desktop/ss.mp4";
    ctx->input_media[0] = input_info;

//    FILE *pcm_file = fopen("/Users/ms/Desktop/ss.pcm", "wb");

    media_info *out_info = (media_info *) malloc(sizeof(media_info));
    memset(out_info, 0, sizeof(out_info));
    out_info->file_name = "/Users/ms/Desktop/ss.mkv";
    ctx->output_media[0] = out_info;

    for (int i = 0; i < sizeof(ctx->input_media) / sizeof(ctx->input_media[0]); i++)
        if (open_input_file(ctx->input_media[i]) < 0)
            return -1;

    for (int i = 0; i < sizeof(ctx->output_media) / sizeof(ctx->output_media[0]); i++) {
        media_param *param = (media_param *) malloc(sizeof(media_param));
        for (int i = 0; i < AVMEDIA_TYPE_NB; i++) {
            AVCodecContext *avctx = ctx->input_media[0]->avctx[i];
            if (avctx) {
                switch (avctx->codec_type) {
                    case AVMEDIA_TYPE_VIDEO:
                        param->width = avctx->width;
                        param->height = avctx->height;
                        param->pix_fmt = avctx->pix_fmt;
                        param->framerate = avctx->framerate;
                        param->sample_aspect_ratio = avctx->sample_aspect_ratio;
                        param->video_time_base = av_inv_q(avctx->framerate);
                        break;
                    case AVMEDIA_TYPE_AUDIO:
                        param->sample_rate = avctx->sample_rate;
                        param->channels = avctx->channels;
                        param->channel_layout = av_get_default_channel_layout(avctx->channels);
                        param->sample_fmt = avctx->sample_fmt;
//                        param->audio_time_base = (AVRational) {1, avctx->sample_rate};
                        break;
                    case AVMEDIA_TYPE_SUBTITLE:
//                        param->video_time_base = avctx->time_base;
                        break;
                }
            }
        }

        if (open_output_file(ctx->output_media[i], param) < 0)
            return -1;
    }

    //解码
    int ret = -1;
    AVPacket *packet = av_packet_alloc();
    TT tt[1000];
    int count = 0;
    media_info *input = ctx->input_media[0];
    media_info *output = ctx->output_media[0];

    while (1) {
        if ((ret = av_read_frame(input->ic, packet)) < 0)
            break;
        int stream_index = packet->stream_index;
        int stream_out = -1;
        for (int i = 0; i < output->ic->nb_streams; i++) {
            if (input->avctx[stream_index]->codec_type == output->avctx[i]->codec_type) {
                stream_out = i;
                break;
            }
        }
        if (stream_out == -1) {
            av_packet_unref(packet);
            continue;
        }
        input->avctx[stream_index]->pkt_timebase = input->ic->streams[stream_index]->time_base;
//        av_packet_rescale_ts(packet,
//                             input->ic->streams[stream_index]->time_base,
//                             input->avctx[stream_index]->time_base);
        ret = avcodec_send_packet(input->avctx[stream_index], packet);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Decoding failed ret:%s\n", av_err2str(ret));
            break;
        }

        while (ret >= 0) {
            tt[count].frame = av_frame_alloc();
            tt[count].input_index = stream_index;
            tt[count].output_index = stream_out;

            ret = avcodec_receive_frame(input->avctx[stream_index], tt[count].frame);
//            av_log(NULL, AV_LOG_ERROR, "Decoding ret:%d\n", ret);
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                break;
            else if (ret < 0)
                return -1;

//            double enP = 0;
//            if (tt[count].frame)
//                enP = tt[count].frame->pts * av_q2d(input_info->avctx[stream_index]->time_base);
//            av_log(NULL, AV_LOG_INFO, "Encoding frame %d ret:%f\n", stream_index, enP);

//            if (input->avctx[stream_index]->codec_type == AVMEDIA_TYPE_VIDEO) {
//                char buf[1024];
//                snprintf(buf, sizeof(buf), "%s-%d.jpg", "/Users/ms/Desktop/pic/ss", input->avctx[stream_index]->frame_number);
//                pgm_save(input->frame[stream_index]->data[0], input->frame[stream_index]->linesize[0],
//                         input->frame[stream_index]->width, input->frame[stream_index]->height, buf);
//            } else if (input->avctx[stream_index]->codec_type == AVMEDIA_TYPE_AUDIO) {
//                int data_size = av_get_bytes_per_sample(input->avctx[stream_index]->sample_fmt);
//                if (data_size < 0) {
//                    /* This should not occur, checking just for paranoia */
//                    fprintf(stderr, "Failed to calculate data size\n");
//                    exit(1);
//                }
//                for (int i = 0; i < input->frame[stream_index]->nb_samples; i++)
//                    for (int ch = 0; ch < input->avctx[stream_index]->channels; ch++)
//                        fwrite(input->frame[stream_index]->data[ch] + data_size * i, 1, data_size, pcm_file);
//            }
            count++;
        }
//        encode_write_frame(output, input, stream_index, nullptr);
    }

    for (int i = 0; i < count; i++) {
        encode_write_frame(output, input, &tt[i]);
    }
    for (int i = 0; i < output->ic->nb_streams; i++) {
        TT tmp;
        tmp.output_index = i;
        tmp.frame = nullptr;
        encode_write_frame(output, input, &tmp);
    }
//    encode_write_frame(output, input, 1, nullptr);
    av_write_trailer(output->ic);

//    fclose(pcm_file);

    printf("==end\n");
    return 0;
}
