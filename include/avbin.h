/* avbin.h
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

#ifndef AVBIN_H
#define AVBIN_H

#include <stdint.h>

typedef enum _AVbinResult {
    AVBIN_RESULT_ERROR = -1,
    AVBIN_RESULT_OK = 0
} AVbinResult;

typedef enum _AVbinStreamType {
    AVBIN_STREAM_TYPE_UNKNOWN = 0,
    AVBIN_STREAM_TYPE_VIDEO = 1,
    AVBIN_STREAM_TYPE_AUDIO = 2
} AVbinStreamType;

typedef enum _AVbinSampleFormat {
    AVBIN_SAMPLE_FORMAT_U8 = 0,
    AVBIN_SAMPLE_FORMAT_S16 = 1,
    AVBIN_SAMPLE_FORMAT_S24 = 2,
    AVBIN_SAMPLE_FORMAT_S32 = 3,
    AVBIN_SAMPLE_FORMAT_FLOAT = 4
} AVbinSampleFormat;

typedef enum _AVbinLogLevel {
    AVBIN_LOG_QUIET = -8,
    AVBIN_LOG_PANIC = 0,
    AVBIN_LOG_FATAL = 8,
    AVBIN_LOG_ERROR = 16,
    AVBIN_LOG_WARNING = 24,
    AVBIN_LOG_INFO = 32,
    AVBIN_LOG_VERBOSE = 40,
    AVBIN_LOG_DEBUG = 48
} AVbinLogLevel;

typedef struct _AVbinFile AVbinFile;
typedef struct _AVbinStream AVbinStream;

typedef int64_t Timestamp; /* Timestamps are always in microseconds */

typedef struct _AVbinFileInfo {
    size_t structure_size;
    int n_streams;

    Timestamp start_time;
    Timestamp duration;

    char title[512];
    char author[512];
    char copyright[512];
    char comment[512];
    char album[512];
    int year;
    int track;
    char genre[32];
} AVbinFileInfo;

typedef struct _AVbinStreamInfo {
    size_t structure_size;
    AVbinStreamType type;

    union {
        struct {
            unsigned int width;
            unsigned int height;
            unsigned int sample_aspect_num;
            unsigned int sample_aspect_den;
        } video;

        struct {
            AVbinSampleFormat sample_format;
            unsigned int sample_rate;
            unsigned int sample_bits;
            unsigned int channels;
        } audio;
    };
} AVbinStreamInfo;

typedef struct _AVbinPacket {
    size_t structure_size;
    Timestamp timestamp;
    int stream_index;

    uint8_t *data;
    size_t size;
} AVbinPacket;

typedef void (*AVbinLogCallback)(const char *module, 
                                 int level, 
                                 const char *message);

int avbin_get_version();
int avbin_get_ffmpeg_revision();
size_t avbin_audio_buffer_size();
int avbin_have_feature(const char *feature);

AVbinResult avbin_init();
AVbinResult avbin_set_log_level(AVbinLogLevel level);
AVbinResult avbin_set_log_callback(AVbinLogCallback callback);

AVbinFile *avbin_open_filename(const char *filename);
void avbin_close_file(AVbinFile *file);
AVbinResult avbin_seek_file(AVbinFile *file, Timestamp timestamp);
AVbinResult avbin_file_info(AVbinFile *file,
                            AVbinFileInfo *info);
AVbinResult avbin_stream_info(AVbinFile *file, int stream_index,
                              AVbinStreamInfo *info);
                       

AVbinStream *avbin_open_stream(AVbinFile *file, int stream_index);
void avbin_close_stream(AVbinStream *stream);

AVbinResult avbin_read(AVbinFile *file, AVbinPacket *packet);
int avbin_decode_audio(AVbinStream *stream,
                       uint8_t *data_in, size_t size_in,
                       uint8_t *data_out, int *size_out);
int avbin_decode_video(AVbinStream *stream,
                       uint8_t *data_in, size_t size_in,
                       uint8_t *data_out);
#endif
