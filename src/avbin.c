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

#include <stdlib.h>

#include <avbin.h>

/* ffmpeg */
#include <avformat.h>
#include <avcodec.h>
#include <avutil.h>

struct AVbinFile {
    AVFormatContext *context;
    AVPacket *packet;
};

struct AVbinStream {
    int type;
    AVFormatContext *format_context;
    AVCodecContext *codec_context;
    AVFrame *frame;
};


int avbin_get_version()
{
    avcodec_init();
    return AVBIN_VERSION;
}

int avbin_init()
{
    av_register_all();    
}

size_t avbin_audio_buffer_size()
{
    return AVCODEC_MAX_AUDIO_FRAME_SIZE;
}

struct AVbinFile *avbin_open_filename(const char *filename)
{
    struct AVbinFile *file = malloc(sizeof *file);
    if (av_open_input_file(&file->context, filename, NULL, 0, NULL) != 0)
        goto error;

    if (av_find_stream_info(file->context) < 0)
        goto error;

    dump_format(file->context, 0, filename, 0);

    file->packet = NULL;
    return file;

error:
    free(file);
    return NULL;
}

void avbin_close_file(struct AVbinFile *file)
{
    if (file->packet)
    {
        av_free_packet(file->packet);
        free(file->packet);
    }
    av_close_input_file(file->context);
}

void avbin_seek_file(struct AVbinFile *file, Timestamp timestamp)
{
    av_seek_frame(file->context, -1, timestamp, 0);
}

void avbin_file_info(struct AVbinFile *file, struct AVbinFileInfo *info)
{
    if (info->structure_size < sizeof *info)
        return;

    info->streams = file->context->nb_streams;
    info->duration = file->context->duration;
    memcpy(info->title, file->context->title, sizeof(info->title));
    memcpy(info->author, file->context->author, sizeof(info->author));
    memcpy(info->copyright, file->context->copyright, sizeof(info->copyright));
    memcpy(info->comment, file->context->comment, sizeof(info->comment));
    memcpy(info->album, file->context->album, sizeof(info->album));
    info->year = file->context->year;
    info->track = file->context->track;
    memcpy(info->genre, file->context->genre, sizeof(info->genre));
}


int codec_context_stream_type(int type)
{
    switch (type)
    {
        case CODEC_TYPE_VIDEO:
            return AVBIN_STREAM_TYPE_VIDEO;
        case CODEC_TYPE_AUDIO:
            return AVBIN_STREAM_TYPE_AUDIO;
        default:
            return AVBIN_STREAM_TYPE_UNKNOWN;
    }

}

int avbin_get_stream_type(struct AVbinFile *file, int index)
{
    if (index < 0 || index >= file->context->nb_streams)
        return AVBIN_STREAM_TYPE_UNKNOWN;

    return codec_context_stream_type(
        file->context->streams[index]->codec->codec_type);
}

struct AVbinStream *avbin_open_stream(struct AVbinFile *file, int index) 
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

    struct AVbinStream *stream = malloc(sizeof *stream);
    stream->format_context = file->context;
    stream->codec_context = codec_context;
    stream->type = codec_context_stream_type(codec_context->codec_type);
    if (stream->type == AVBIN_STREAM_TYPE_VIDEO)
        stream->frame = avcodec_alloc_frame();
    else
        stream->frame = NULL;
    return stream;
}

void avbin_close_stream(struct AVbinStream *stream)
{
    if (stream->frame)
        av_free(stream->frame);
    avcodec_close(stream->codec_context);
}

void avbin_stream_info(struct AVbinStream *stream,
                       struct AVbinStreamInfo *info)
{
    /* This is the first version, so anything smaller is an error. */
    if (info->structure_size < sizeof *info)
        return;

    info->type = stream->type;
    switch (stream->type) 
    {
        case AVBIN_STREAM_TYPE_VIDEO:
            info->video.width = stream->codec_context->width;
            info->video.height = stream->codec_context->height;
            info->video.sample_aspect_num = 
                stream->codec_context->sample_aspect_ratio.num;
            info->video.sample_aspect_den = 
                stream->codec_context->sample_aspect_ratio.den;
            break;

        case AVBIN_STREAM_TYPE_AUDIO:
            info->audio.sample_rate = stream->codec_context->sample_rate;
            info->audio.channels = stream->codec_context->channels;
            switch (stream->codec_context->sample_fmt)
            {
                case SAMPLE_FMT_U8:
                    info->audio.sample_rate = AVBIN_SAMPLE_FORMAT_U8;
                    info->audio.sample_size = 8;
                    break;
                case SAMPLE_FMT_S16:
                    info->audio.sample_format = AVBIN_SAMPLE_FORMAT_S16;
                    info->audio.sample_size = 16;
                    break;
                case SAMPLE_FMT_S24:
                    info->audio.sample_format = AVBIN_SAMPLE_FORMAT_S24;
                    info->audio.sample_size = 24;
                    break;
                case SAMPLE_FMT_S32:
                    info->audio.sample_format = AVBIN_SAMPLE_FORMAT_S32;
                    info->audio.sample_size = 32;
                    break;
                case SAMPLE_FMT_FLT:
                    info->audio.sample_format = AVBIN_SAMPLE_FORMAT_FLOAT;
                    info->audio.sample_size = 32;
                    break;
            }
            break;
    }
}

int avbin_read(struct AVbinFile *file, struct AVbinPacket *packet)
{
    if (packet->structure_size < sizeof *packet)
        return -1;

    if (file->packet)
        av_free_packet(file->packet);
    else
        file->packet = malloc(sizeof *file->packet);

    if (av_read_frame(file->context, file->packet) < 0)
        return -1;

    packet->has_timestamp = file->packet->dts != AV_NOPTS_VALUE;
    packet->timestamp = av_rescale_q(file->packet->dts,
        file->context->streams[file->packet->stream_index]->time_base,
        AV_TIME_BASE_Q);
    packet->stream = file->packet->stream_index;
    packet->data = file->packet->data;
    packet->size = file->packet->size;

    return 0;
}

int avbin_decode_audio(struct AVbinStream *stream,
                       uint8_t *data_in, size_t size_in,
                       uint8_t *data_out, int *size_out)
{
    int used;
    used = avcodec_decode_audio2(stream->codec_context, 
                                 (int16_t *) data_out, size_out,
                                 data_in, size_in);

    if (used < 0)
        return -1;

    return used;
}

int avbin_decode_video(struct AVbinStream *stream,
                       uint8_t *data_in, size_t size_in,
                       uint8_t *data_out, int *pitch)
{
    AVPicture picture_rgb;
    int got_picture;
    int width = stream->codec_context->width;
    int height = stream->codec_context->height;
    int used;

    used = avcodec_decode_video(stream->codec_context, 
                                stream->frame, &got_picture,
                                data_in, size_in);
    if (!got_picture)
        return -1;


    avpicture_fill(&picture_rgb, data_out, PIX_FMT_RGB24, width, height);
    *pitch = picture_rgb.linesize[0];

    /* img_convert is marked deprecated in favour of swscale, don't
     * be surprised if this stops working the next time the ffmpeg version
     * is pushed.  Example use of the new API is in ffplay.c. */
    img_convert(&picture_rgb, PIX_FMT_RGB24, 
                (AVPicture *) stream->frame, stream->codec_context->pix_fmt,
                width, height);
    
    return used;
}

