/*
 * IEC 61937 common header
 * Copyright (c) 2009 Bartlomiej Wolowiec
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

#include <stdint.h>

#define SYNCWORD1 0xF872
#define SYNCWORD2 0x4E1F
#define BURST_HEADER_SIZE 0x8

enum IEC958DataType {
    IEC958_AC3                = 0x01,          ///< AC-3 data
    IEC958_MPEG1_LAYER1       = 0x04,          ///< MPEG-1 layer 1
    IEC958_MPEG1_LAYER23      = 0x05,          ///< MPEG-1 layer 2 or 3 data or MPEG-2 without extension
    IEC958_MPEG2_EXT          = 0x06,          ///< MPEG-2 data with extension
    IEC958_MPEG2_AAC          = 0x07,          ///< MPEG-2 AAC ADTS
    IEC958_MPEG2_LAYER1_LSF   = 0x08,          ///< MPEG-2, layer-1 low sampling frequency
    IEC958_MPEG2_LAYER2_LSF   = 0x09,          ///< MPEG-2, layer-2 low sampling frequency
    IEC958_MPEG2_LAYER3_LSF   = 0x0A,          ///< MPEG-2, layer-3 low sampling frequency
    IEC958_DTS1               = 0x0B,          ///< DTS type I   (512 samples)
    IEC958_DTS2               = 0x0C,          ///< DTS type II  (1024 samples)
    IEC958_DTS3               = 0x0D,          ///< DTS type III (2048 samples)
    IEC958_ATRAC              = 0x0E,          ///< Atrac data
    IEC958_ATRAC3             = 0x0F,          ///< Atrac 3 data
    IEC958_ATRACX             = 0x10,          ///< Atrac 3 plus data
    IEC958_DTSHD              = 0x11,          ///< DTS HD data
    IEC958_WMAPRO             = 0x12,          ///< WMA 9 Professional data
    IEC958_MPEG2_AAC_LSF_2048 = 0x13,          ///< MPEG-2 AAC ADTS half-rate low sampling frequency
    IEC958_MPEG2_AAC_LSF_4096 = 0x13 | 0x20,   ///< MPEG-2 AAC ADTS quarter-rate low sampling frequency
    IEC958_EAC3               = 0x15,          ///< E-AC-3 data
    IEC958_TRUEHD             = 0x16,          ///< TrueHD data
};

static const uint16_t spdif_mpeg_pkt_offset[2][3] = {
    //LAYER1  LAYER2  LAYER3
    { 3072,    9216,   4608 }, // MPEG2 LSF
    { 1536,    4608,   4608 }, // MPEG1
};

void ff_spdif_bswap_buf16(uint16_t *dst, const uint16_t *src, int w);
