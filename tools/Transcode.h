//
// Created by ms on 2021/2/17.
//

#ifndef VIDEO_EDIT_TRANSCODE_H
#define VIDEO_EDIT_TRANSCODE_H

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

typedef struct _VideoState {
    AVFormatContext *ic;
    AVCodec *video_codec;
    AVCodec *audio_codec;
    AVCodecContext *video_ctx;
    AVCodecContext *audio_ctx;
    int video_stream;
    int audio_stream;
} VideoState;

class Transcode {
public:
    int init(const char *srcPath, const char *dstPath);
    int trans();

private:
    VideoState *is;
};


#endif //VIDEO_EDIT_TRANSCODE_H
