/*
 * RAW Ingenient MJPEG demuxer
 * Copyright (c) 2005 Alex Beregszaszi
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "avformat.h"
#include "rawdec.h"

// http://www.artificis.hu/files/texts/ingenient.txt
static int ingenient_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    int ret, size, w, h, unk1, unk2;

    if (get_le32(s->pb) != MKTAG('M', 'J', 'P', 'G'))
        return AVERROR(EIO); // FIXME

    size = get_le32(s->pb);

    w = get_le16(s->pb);
    h = get_le16(s->pb);

    url_fskip(s->pb, 8); // zero + size (padded?)
    url_fskip(s->pb, 2);
    unk1 = get_le16(s->pb);
    unk2 = get_le16(s->pb);
    url_fskip(s->pb, 22); // ASCII timestamp

    av_log(s, AV_LOG_DEBUG, "Ingenient packet: size=%d, width=%d, height=%d, unk1=%d unk2=%d\n",
        size, w, h, unk1, unk2);

    if (av_new_packet(pkt, size) < 0)
        return AVERROR(ENOMEM);

    pkt->pos = url_ftell(s->pb);
    pkt->stream_index = 0;
    ret = get_buffer(s->pb, pkt->data, size);
    if (ret < 0) {
        av_free_packet(pkt);
        return ret;
    }
    pkt->size = ret;
    return ret;
}

AVInputFormat ingenient_demuxer = {
    "ingenient",
    NULL_IF_CONFIG_SMALL("raw Ingenient MJPEG"),
    0,
    NULL,
    ff_raw_video_read_header,
    ingenient_read_packet,
    .flags= AVFMT_GENERIC_INDEX,
    .extensions = "cgi", // FIXME
    .value = CODEC_ID_MJPEG,
};
