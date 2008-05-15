/* avbin.c
 * Copyright 2007 Alex Holkner
 *
 * This file is part of AVbin.
 *
 * AVbin is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * AVbin is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <avbin.h>

/* ffmpeg */
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>

struct _AVbinFile {
    AVFormatContext *context;
    AVPacket *packet;
};

struct _AVbinStream {
    int type;
    AVFormatContext *format_context;
    AVCodecContext *codec_context;
    AVFrame *frame;
};

static AVbinLogCallback user_log_callback = NULL;

/**
 * Format log messages and call the user log callback.  Essentially a
 * reimplementation of libavutil/log.c:av_log_default_callback.
 */
static void avbin_log_callback(void *ptr, 
                               int level, 
                               const char *fmt, 
                               va_list vl)
{
    static char message[8192];
    const char *module = NULL;

    if (level > av_log_level || !user_log_callback)
        return;

    if (ptr)
    {
        AVClass *avc = *(AVClass**) ptr;
        module = avc->item_name(ptr);
    }

    vsnprintf(message, sizeof message, fmt, vl);
    user_log_callback(module, (AVbinLogLevel) level, message);
}

int avbin_get_version()
{
    return AVBIN_VERSION;
}

int avbin_get_ffmpeg_revision()
{
    return FFMPEG_REVISION;
}

size_t avbin_get_audio_buffer_size()
{
    return AVCODEC_MAX_AUDIO_FRAME_SIZE;
}

int avbin_have_feature(const char *feature)
{
    return 0;
}

AVbinResult avbin_init()
{
    avcodec_init();
    av_register_all();    
    return AVBIN_RESULT_OK;
}

AVbinResult avbin_set_log_level(AVbinLogLevel level)
{
    av_log_level = level;
    return AVBIN_RESULT_OK;
}

AVbinResult avbin_set_log_callback(AVbinLogCallback callback)
{
    user_log_callback = callback;

    /* Note av_log_set_callback looks set to disappear at 
     * LIBAVUTIL_VERSION >= 50; at which point av_vlog must be
     * set directly.
     */
    if (callback)
        av_log_set_callback(avbin_log_callback);
    else
        av_log_set_callback(av_log_default_callback);
    return AVBIN_RESULT_OK;
}

AVbinFile *avbin_open_filename(const char *filename)
{
    AVbinFile *file = malloc(sizeof *file);
    if (av_open_input_file(&file->context, filename, NULL, 0, NULL) != 0)
        goto error;

    if (av_find_stream_info(file->context) < 0)
        goto error;

    file->packet = NULL;
    return file;

error:
    free(file);
    return NULL;
}

void avbin_close_file(AVbinFile *file)
{
    if (file->packet)
    {
        av_free_packet(file->packet);
        free(file->packet);
    }
    av_close_input_file(file->context);
}

AVbinResult avbin_seek_file(AVbinFile *file, AVbinTimestamp timestamp)
{
    int i;
    AVCodecContext *codec_context;

    if (!timestamp)
        av_seek_frame(file->context, -1, 0, 
                      AVSEEK_FLAG_ANY | AVSEEK_FLAG_BYTE);
    else
        av_seek_frame(file->context, -1, timestamp, 0);

    for (i = 0; i < file->context->nb_streams; i++)
    {
        codec_context = file->context->streams[i]->codec;
        if (codec_context)
            avcodec_flush_buffers(codec_context);
    }
    return AVBIN_RESULT_OK;
}

AVbinResult avbin_file_info(AVbinFile *file, AVbinFileInfo *info)
{
    if (info->structure_size < sizeof *info)
        return AVBIN_RESULT_ERROR;

    info->n_streams = file->context->nb_streams;
    info->start_time = file->context->start_time;
    info->duration = file->context->duration;
    memcpy(info->title, file->context->title, sizeof(info->title));
    memcpy(info->author, file->context->author, sizeof(info->author));
    memcpy(info->copyright, file->context->copyright, sizeof(info->copyright));
    memcpy(info->comment, file->context->comment, sizeof(info->comment));
    memcpy(info->album, file->context->album, sizeof(info->album));
    info->year = file->context->year;
    info->track = file->context->track;
    memcpy(info->genre, file->context->genre, sizeof(info->genre));

    return AVBIN_RESULT_OK;
}

int avbin_stream_info(AVbinFile *file, int stream_index,
                      AVbinStreamInfo *info)
{
    AVCodecContext *context = file->context->streams[stream_index]->codec;

    /* This is the first version, so anything smaller is an error. */
    if (info->structure_size < sizeof *info)
        return AVBIN_RESULT_ERROR;

    switch (context->codec_type)
    {
        case CODEC_TYPE_VIDEO:
            info->type = AVBIN_STREAM_TYPE_VIDEO;
            info->video.width = context->width;
            info->video.height = context->height;
            info->video.sample_aspect_num = context->sample_aspect_ratio.num;
            info->video.sample_aspect_den = context->sample_aspect_ratio.den;
            break;
        case CODEC_TYPE_AUDIO:
            info->type = AVBIN_STREAM_TYPE_AUDIO;
            info->audio.sample_rate = context->sample_rate;
            info->audio.channels = context->channels;
            switch (context->sample_fmt)
            {
                case SAMPLE_FMT_U8:
                    info->audio.sample_rate = AVBIN_SAMPLE_FORMAT_U8;
                    info->audio.sample_bits = 8;
                    break;
                case SAMPLE_FMT_S16:
                    info->audio.sample_format = AVBIN_SAMPLE_FORMAT_S16;
                    info->audio.sample_bits = 16;
                    break;
                case SAMPLE_FMT_S24:
                    info->audio.sample_format = AVBIN_SAMPLE_FORMAT_S24;
                    info->audio.sample_bits = 24;
                    break;
                case SAMPLE_FMT_S32:
                    info->audio.sample_format = AVBIN_SAMPLE_FORMAT_S32;
                    info->audio.sample_bits = 32;
                    break;
                case SAMPLE_FMT_FLT:
                    info->audio.sample_format = AVBIN_SAMPLE_FORMAT_FLOAT;
                    info->audio.sample_bits = 32;
                    break;
            }
            break;

        default:
            info->type = AVBIN_STREAM_TYPE_UNKNOWN;
    }

    return AVBIN_RESULT_OK;
}

AVbinStream *avbin_open_stream(AVbinFile *file, int index) 
{
    AVCodecContext *codec_context;
    AVCodec *codec;

    if (index < 0 || index >= file->context->nb_streams)
        return NULL;

    codec_context = file->context->streams[index]->codec;
    codec = avcodec_find_decoder(codec_context->codec_id);
    if (!codec)
        return NULL;

    if (avcodec_open(codec_context, codec) < 0)
        return NULL;

    AVbinStream *stream = malloc(sizeof *stream);
    stream->format_context = file->context;
    stream->codec_context = codec_context;
    stream->type = codec_context->codec_type;
    if (stream->type == CODEC_TYPE_VIDEO)
        stream->frame = avcodec_alloc_frame();
    else
        stream->frame = NULL;
    return stream;
}

void avbin_close_stream(AVbinStream *stream)
{
    if (stream->frame)
        av_free(stream->frame);
    avcodec_close(stream->codec_context);
}

int avbin_read(AVbinFile *file, AVbinPacket *packet)
{
    if (packet->structure_size < sizeof *packet)
        return AVBIN_RESULT_ERROR;

    if (file->packet)
        av_free_packet(file->packet);
    else
        file->packet = malloc(sizeof *file->packet);

    if (av_read_frame(file->context, file->packet) < 0)
        return AVBIN_RESULT_ERROR;

    packet->timestamp = av_rescale_q(file->packet->dts,
        file->context->streams[file->packet->stream_index]->time_base,
        AV_TIME_BASE_Q);
    packet->stream_index = file->packet->stream_index;
    packet->data = file->packet->data;
    packet->size = file->packet->size;

    return AVBIN_RESULT_OK;
}

int avbin_decode_audio(AVbinStream *stream,
                       uint8_t *data_in, size_t size_in,
                       uint8_t *data_out, int *size_out)
{
    int used;
    if (stream->type != CODEC_TYPE_AUDIO)
        return AVBIN_RESULT_ERROR;

    used = avcodec_decode_audio2(stream->codec_context, 
                                 (int16_t *) data_out, size_out,
                                 data_in, size_in);

    if (used < 0)
        return AVBIN_RESULT_ERROR;

    return used;
}

int avbin_decode_video(AVbinStream *stream,
                       uint8_t *data_in, size_t size_in,
                       uint8_t *data_out)
{
    AVPicture picture_rgb;
    int got_picture;
    int width = stream->codec_context->width;
    int height = stream->codec_context->height;
    int used;

    if (stream->type != CODEC_TYPE_VIDEO)
        return AVBIN_RESULT_ERROR;

    used = avcodec_decode_video(stream->codec_context, 
                                stream->frame, &got_picture,
                                data_in, size_in);
    if (!got_picture)
        return AVBIN_RESULT_ERROR;


    avpicture_fill(&picture_rgb, data_out, PIX_FMT_RGB24, width, height);

    /* img_convert is marked deprecated in favour of swscale, don't
     * be surprised if this stops working the next time the ffmpeg version
     * is pushed.  Example use of the new API is in ffplay.c. */
    img_convert(&picture_rgb, PIX_FMT_RGB24, 
                (AVPicture *) stream->frame, stream->codec_context->pix_fmt,
                width, height);
    
    return used;
}

