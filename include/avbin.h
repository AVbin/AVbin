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

#define AVBIN_VERSION 1

#define AVBIN_STREAM_TYPE_UNKNOWN 0
#define AVBIN_STREAM_TYPE_VIDEO 1
#define AVBIN_STREAM_TYPE_AUDIO 2

#define AVBIN_SAMPLE_FORMAT_U8 0
#define AVBIN_SAMPLE_FORMAT_S16 1
#define AVBIN_SAMPLE_FORMAT_S24 2
#define AVBIN_SAMPLE_FORMAT_S32 3
#define AVBIN_SAMPLE_FORMAT_FLOAT 4

struct AVbinFile;
struct AVbinStream;

typedef int64_t Timestamp; /* Timestamps are always in microseconds */

struct AVbinFileInfo {
    size_t structure_size;
    int streams;

    Timestamp duration;

    char title[512];
    char author[512];
    char copyright[512];
    char comment[512];
    char album[512];
    int year;
    int track;
    char genre[32];
};

struct AVbinStreamInfo {
    size_t structure_size;
    int type; /* One of the STREAM_TYPE_* constants */

    union {
        struct {
            unsigned int width;
            unsigned int height;
            unsigned int sample_aspect_num;
            unsigned int sample_aspect_den;
        } video;

        struct {
            unsigned int sample_rate;
            unsigned int channels;
            unsigned int sample_size;  /* Bits per sample (typically 8 or 16)*/
            int sample_format; /* One of the SAMPLE_FORMAT_* constants */
        } audio;
    };
};

struct AVbinPacket {
    size_t structure_size;
    int has_timestamp;
    Timestamp timestamp;
    int stream;

    uint8_t *data;
    size_t size;
};

int avbin_get_version();
int avbin_init();
size_t avbin_audio_buffer_size();

struct AVbinFile *avbin_open_filename(const char *filename);
void avbin_close_file(struct AVbinFile *file);
void avbin_seek_file(struct AVbinFile *file, Timestamp timestamp);
void avbin_file_info(struct AVbinFile *file,
                     struct AVbinFileInfo *info);

int avbin_read(struct AVbinFile *file, struct AVbinPacket *packet);
int avbin_decode_audio(struct AVbinStream *stream,
                       uint8_t *data_in, size_t size_in,
                       uint8_t *data_out, int *size_out);
int avbin_decode_video(struct AVbinStream *stream,
                       uint8_t *data_in, size_t size_in,
                       uint8_t *data_out, int *pitch);
int avbin_get_stream_count(struct AVbinFile *file);
int avbin_get_stream_type(struct AVbinFile *file, int index);

struct AVbinStream *avbin_open_stream(struct AVbinFile *file, int index);
void avbin_stream_info(struct AVbinStream *stream,
                       struct AVbinStreamInfo *info);
void avbin_close_stream(struct AVbinStream *stream);


#endif
