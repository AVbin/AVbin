/* avbin.c
 * Copyright 2012 AVbin Team
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

/* libav */
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/dict.h>
#include <libavutil/mathematics.h>
#include <libswscale/swscale.h>

/* XXX TODO XXX
 * - Use av_find_best_stream() somehow useful.
 *   http://libav.org/doxygen/master/group__lavf__decoding.html#gaa6fa468c922ff5c60a6021dcac09aff9
 * - Try using avformat_seek_file() instead of av_seek_frame() for seeking.
 *   http://libav.org/doxygen/master/group__lavf__decoding.html#ga3b40fc8d2fda6992ae6ea2567d71ba30
 */

static int32_t avbin_thread_count = 1;

struct _AVbinFile {
    AVFormatContext *context;
    AVPacket *packet;
    PtsCorrectionContext pts_correction_context;
};

struct _AVbinStream {
    int32_t type;
    AVFormatContext *format_context;
    AVCodecContext *codec_context;
    AVFrame *frame;
};

static AVbinLogCallback user_log_callback = NULL;

/* (Pulled straight from libav/cmdutils.c, since I couldn't find a clean way to
 * include it from cmdutils.o without pulling in lots of other stuff too.)
 */
int64_t guess_correct_pts(PtsCorrectionContext *ctx, int64_t reordered_pts,
                          int64_t dts)
{
    int64_t pts = AV_NOPTS_VALUE;

    if (dts != AV_NOPTS_VALUE) {
        ctx->num_faulty_dts += dts <= ctx->last_dts;
        ctx->last_dts = dts;
    }
    if (reordered_pts != AV_NOPTS_VALUE) {
        ctx->num_faulty_pts += reordered_pts <= ctx->last_pts;
        ctx->last_pts = reordered_pts;
    }
    if ((ctx->num_faulty_pts<=ctx->num_faulty_dts || dts == AV_NOPTS_VALUE)
        && reordered_pts != AV_NOPTS_VALUE)
        pts = reordered_pts;
    else
        pts = dts;

    return pts;
}


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

//    if (level > av_log_level || !user_log_callback)
    if (level > av_log_get_level() || !user_log_callback)
        return;

    if (ptr)
    {
        AVClass *avc = *(AVClass**) ptr;
        module = avc->item_name(ptr);
    }

    vsnprintf(message, sizeof message, fmt, vl);
    user_log_callback(module, (AVbinLogLevel) level, message);
}

int32_t avbin_get_version()
{
    return AVBIN_VERSION;
}

AVbinInfo *avbin_get_info()
{
    AVbinInfo *info = malloc(sizeof(*info));

    info->structure_size         = sizeof(*info);
    info->version                = avbin_get_version();
    info->version_string         = AVBIN_VERSION_STRING;
    info->build_date             = AVBIN_BUILD_DATE;
    info->repo                   = AVBIN_REPO;
    info->commit                 = AVBIN_COMMIT;
    info->backend                = AVBIN_BACKEND;
    info->backend_version_string = AVBIN_BACKEND_VERSION_STRING;
    info->backend_repo           = AVBIN_BACKEND_REPO;
    info->backend_commit         = AVBIN_BACKEND_COMMIT;
    return info;
}

// Deprecated - use avbin_get_info() instead.  Will be removed in version 12.
int32_t avbin_get_ffmpeg_revision()
{
    return 0;
}

// Deprecated - useless.  Will be removed in version 13.
size_t avbin_get_audio_buffer_size()
{
    return 192000;
}

int32_t avbin_have_feature(const char *feature)
{
    if (strcmp(feature, "frame_rate") == 0)
    {
        // See note on avbin_have_feature() in avbin.h
        return 0;
    }
    if (strcmp(feature, "options") == 0)
        return 1;
    if (strcmp(feature, "info") == 0)
        return 1;
    return 0;
}

AVbinResult avbin_init()
{
    return avbin_init_options(NULL);
}

AVbinResult avbin_init_options(AVbinOptions * options_ptr)
{
    if (options_ptr == NULL)
    {
        options_ptr = malloc(sizeof(options_ptr));
        if (options_ptr == NULL)
            return AVBIN_RESULT_ERROR;

        // Set defaults...
        options_ptr->structure_size = sizeof(AVbinOptions);
        options_ptr->thread_count = 1;
    }

    // What version did we get?
    AVbinOptions * options = NULL;
    if (options_ptr->structure_size == sizeof(AVbinOptions))
    {
        options = options_ptr;
    } else {
        return AVBIN_RESULT_ERROR;
    }

    // Stupid choices deserve single-threading
    if (options->thread_count < 0)
        options->thread_count = 1;

    avbin_thread_count = options->thread_count;

    av_register_all();
    avcodec_register_all();

    return AVBIN_RESULT_OK;
}

AVbinResult avbin_set_log_level(AVbinLogLevel level)
{
    av_log_set_level(level);
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

AVbinFile *avbin_open_filename(const char *filename) { return avbin_open_filename_with_format(filename, NULL); }

AVbinFile *avbin_open_filename_with_format(const char *filename, char* format)
{
    AVbinFile *file = malloc(sizeof *file);
    AVInputFormat *avformat = NULL;
    if (format) avformat = av_find_input_format(format);

    file->context = NULL;
    /* file->context = avformat_alloc_context(); */
    if (avformat_open_input(&file->context, filename, avformat, NULL) != 0)
        goto error;

    if (avformat_find_stream_info(file->context, NULL) < 0)
      goto error;

    file->packet = NULL;
    file->pts_correction_context.num_faulty_pts = 0;
    file->pts_correction_context.num_faulty_dts = 0;
    file->pts_correction_context.last_pts       = INT64_MIN;
    file->pts_correction_context.last_dts       = INT64_MIN;

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

    avformat_close_input(&file->context);
    free(file);
}

AVbinResult avbin_seek_file(AVbinFile *file, AVbinTimestamp timestamp)
{
    int i;
    AVCodecContext *codec_context;
    int flags = 0;

    if (!timestamp)
    {
        flags = AVSEEK_FLAG_ANY | AVSEEK_FLAG_BYTE;
        if (av_seek_frame(file->context, -1, 0, flags) < 0)
            return AVBIN_RESULT_ERROR;
    }
    else
    {
        flags = AVSEEK_FLAG_BACKWARD;
        if (av_seek_frame(file->context, -1, timestamp, flags) < 0)
            return AVBIN_RESULT_ERROR;
    }

    for (i = 0; i < file->context->nb_streams; i++)
    {
        codec_context = file->context->streams[i]->codec;
        if (codec_context && codec_context->codec)
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

    // Zero-initialize fields first
    memset(info->title, 0, sizeof(info->title));
    memset(info->author, 0, sizeof(info->author));
    memset(info->copyright, 0, sizeof(info->copyright));
    memset(info->comment, 0, sizeof(info->comment));
    memset(info->album, 0, sizeof(info->album));
    memset(info->genre, 0, sizeof(info->genre));
    info->year = 0;
    info->track = 0;

    AVDictionaryEntry* entry;
    if ((entry = av_dict_get(file->context->metadata, "title", NULL, 0)) != NULL)  {
      strncpy(info->title, entry->value, sizeof(info->title));
    }

    if (((entry = av_dict_get(file->context->metadata, "artist", NULL, 0)) != NULL) ||
       (entry = av_dict_get(file->context->metadata, "album_artist", NULL, 0)) != NULL) {
      strncpy(info->author, entry->value, sizeof(info->author));
    }
    if ((entry = av_dict_get(file->context->metadata, "copyright", NULL, 0)) != NULL)  {
      strncpy(info->copyright, entry->value, sizeof(info->copyright));
    }
    if ((entry = av_dict_get(file->context->metadata, "comment", NULL, 0)) != NULL)  {
      strncpy(info->comment, entry->value, sizeof(info->comment));
    }
    if ((entry = av_dict_get(file->context->metadata, "album", NULL, 0)) != NULL)  {
      strncpy(info->album, entry->value, sizeof(info->album));
    }
    if ((entry = av_dict_get(file->context->metadata, "date", NULL, 0)) != NULL)  {
      info->year = atoi(entry->value);
    }
    if ((entry = av_dict_get(file->context->metadata, "track", NULL, 0)) != NULL)  {
      info->track = atoi(entry->value);
    }
    if ((entry = av_dict_get(file->context->metadata, "genre", NULL, 0)) != NULL)  {
      strncpy(info->genre, entry->value, sizeof(info->genre));
    }

    return AVBIN_RESULT_OK;
}

AVbinResult avbin_stream_info(AVbinFile *file, int32_t stream_index,
                      AVbinStreamInfo *info)
{
    AVCodecContext *context = file->context->streams[stream_index]->codec;
    AVbinStreamInfo8 *info_8 = NULL;

    /* Error if not large enough for version 1 */
    if (info->structure_size < sizeof *info)
        return AVBIN_RESULT_ERROR;

    /* Version 8 adds frame_rate feature, Version 11 removes it, see note on
       avbin_have_feature() in avbin.h */
    if (info->structure_size >= sizeof(AVbinStreamInfo8))
        info_8 = (AVbinStreamInfo8 *) info;

    switch (context->codec_type)
    {
        case AVMEDIA_TYPE_VIDEO:
            info->type = AVBIN_STREAM_TYPE_VIDEO;
            info->video.width = context->width;
            info->video.height = context->height;
            info->video.sample_aspect_num = context->sample_aspect_ratio.num;
            info->video.sample_aspect_den = context->sample_aspect_ratio.den;

/* See note on avbin_have_feature() in avbin.h
            if (info_8)
            {
                AVRational frame_rate = \
                    file->context->streams[stream_index]->r_frame_rate;
                info_8->video.frame_rate_num = frame_rate.num;
                info_8->video.frame_rate_den = frame_rate.den;

                // Work around bug in Libav: if frame rate over 1000, divide
                // by 1000.
                if (info_8->video.frame_rate_num /
                        info_8->video.frame_rate_den > 1000)
                    info_8->video.frame_rate_den *= 1000;
            }
*/
            if (info_8)
            {
                info_8->video.frame_rate_num = 0;
                info_8->video.frame_rate_den = 0;
            }
            break;
        case AVMEDIA_TYPE_AUDIO:
            info->type = AVBIN_STREAM_TYPE_AUDIO;
            info->audio.sample_rate = context->sample_rate;
            info->audio.channels = context->channels;
            switch (context->sample_fmt)
            {
                case AV_SAMPLE_FMT_U8:
                    info->audio.sample_format = AVBIN_SAMPLE_FORMAT_U8;
                    info->audio.sample_bits = 8;
                    break;
                case AV_SAMPLE_FMT_S16:
                    info->audio.sample_format = AVBIN_SAMPLE_FORMAT_S16;
                    info->audio.sample_bits = 16;
                    break;
                case AV_SAMPLE_FMT_S32:
                    info->audio.sample_format = AVBIN_SAMPLE_FORMAT_S32;
                    info->audio.sample_bits = 32;
                    break;
                case AV_SAMPLE_FMT_FLT:
                    info->audio.sample_format = AVBIN_SAMPLE_FORMAT_FLOAT;
                    info->audio.sample_bits = 32;
                    break;
                default:
                  // Unknown sample format
                  info->audio.sample_format = -1;
                  info->audio.sample_bits = -1;
                  break;

                // TODO: support planar formats
            }
            break;

        default:
            info->type = AVBIN_STREAM_TYPE_UNKNOWN;
            break;
    }

    return AVBIN_RESULT_OK;
}

AVbinStream *avbin_open_stream(AVbinFile *file, int32_t index)
{
    AVCodecContext *codec_context;
    AVCodec *codec;

    if (index < 0 || index >= file->context->nb_streams)
        return NULL;

    codec_context = file->context->streams[index]->codec;
    codec = avcodec_find_decoder(codec_context->codec_id);
    if (!codec)
        return NULL;

    /* The Libav api example does this (see libav/libavcodec-api-example.c).
     * The only explanation is "we do not send complete frames".  I tried
     * adding it, and there seemed to be no effect either way.  I'm going to
     * leave it here commented out just in case we find the need to enable it
     * in the future.
     */
/*    if (codec->capabilities & CODEC_CAP_TRUNCATED)
 *       codec_context->flags |= CODEC_FLAG_TRUNCATED;
 */
    if (avbin_thread_count != 1)
        codec_context->thread_count = avbin_thread_count;

    if (avcodec_open2(codec_context, codec, NULL) < 0)
        return NULL;

    AVbinStream *stream = malloc(sizeof *stream);
    stream->format_context = file->context;
    stream->codec_context = codec_context;
    stream->type = codec_context->codec_type;
    stream->frame = avcodec_alloc_frame();

    return stream;
}

void avbin_close_stream(AVbinStream *stream)
{
    if (stream->frame)
        avcodec_free_frame(&stream->frame);
    avcodec_close(stream->codec_context);
    free(stream);
}

int32_t avbin_read(AVbinFile *file, AVbinPacket *avbin_packet)
{
    if (avbin_packet->structure_size < sizeof *avbin_packet)
        return AVBIN_RESULT_ERROR;

    if (avbin_packet->av_packet)
        av_free_packet(avbin_packet->av_packet);
    else
        avbin_packet->av_packet = malloc(sizeof *avbin_packet->av_packet);

    av_init_packet(avbin_packet->av_packet);

    /* A packet alias for convenience... (otherwise we have to cast every reference) */
    AVPacket *av_packet_alias = avbin_packet->av_packet;

    if (av_read_frame(file->context, av_packet_alias) < 0)
        return AVBIN_RESULT_ERROR;

    /* A stream alias for convenience... */
    AVStream *stream = file->context->streams[av_packet_alias->stream_index];

    /* Make a timestamp in seconds from beginning of stream */
    avbin_packet->timestamp = av_rescale_q(
        guess_correct_pts(
            &file->pts_correction_context,
            av_packet_alias->pts, av_packet_alias->dts),
        stream->time_base,
        AV_TIME_BASE_Q);
    avbin_packet->stream_index = av_packet_alias->stream_index;
    avbin_packet->data = av_packet_alias->data;
    avbin_packet->size = av_packet_alias->size;

    return AVBIN_RESULT_OK;
}

int32_t avbin_decode_audio(AVbinStream *stream,
                       AVbinPacket *avbin_packet,
                       uint8_t *data_out, int *size_out)
{
    int bytes_used;
    if (stream->type != AVMEDIA_TYPE_AUDIO)
        return AVBIN_RESULT_ERROR;

    int got_frame = 0;
    av_frame_unref(stream->frame);
    bytes_used = avcodec_decode_audio4(
            stream->codec_context, 
            stream->frame, 
            &got_frame, 
            avbin_packet->av_packet);

    if (bytes_used < 0)
        return AVBIN_RESULT_ERROR;

    // TODO: support planar formats
    if (got_frame) {
      int plane_size;
      int data_size = av_samples_get_buffer_size(&plane_size,
                                       stream->codec_context->channels,
                                       stream->frame->nb_samples,
                                       stream->codec_context->sample_fmt, 1);
      if (*size_out < data_size) {
         av_log(stream->codec_context, AV_LOG_ERROR, "Output audio buffer is too small for current audio frame!");
         return AVBIN_RESULT_ERROR;
      }

      memcpy(data_out, stream->frame->extended_data[0], data_size);
      *size_out = data_size;
    } else {
      *size_out = 0;
    }

    return bytes_used;
}

int32_t avbin_decode_video(AVbinStream *stream,
                       AVbinPacket *avbin_packet,
                       uint8_t *data_out)
{
    AVPicture picture_rgb;
    int got_picture;
    int width = stream->codec_context->width;
    int height = stream->codec_context->height;
    int bytes_used;

    if (stream->type != AVMEDIA_TYPE_VIDEO)
        return AVBIN_RESULT_ERROR;

    av_frame_unref(stream->frame);
    bytes_used = avcodec_decode_video2(stream->codec_context,
                                stream->frame, &got_picture,
                                avbin_packet->av_packet);

    if (!got_picture)
        return AVBIN_RESULT_ERROR;


    avpicture_fill(&picture_rgb, data_out, PIX_FMT_RGB24, width, height);
    static struct SwsContext *img_convert_ctx = NULL;
    img_convert_ctx = sws_getCachedContext(img_convert_ctx,width, height,stream->codec_context->pix_fmt,width, height,PIX_FMT_RGB24, SWS_FAST_BILINEAR, NULL, NULL, NULL);
    sws_scale(img_convert_ctx, (const uint8_t* const*)stream->frame->data, stream->frame->linesize,0, height, picture_rgb.data, picture_rgb.linesize);

    return bytes_used;
}
