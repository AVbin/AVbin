/**
 * VP8 compatible video decoder
 *
 * Copyright (C) 2010 David Conrad
 * Copyright (C) 2010 Ronald S. Bultje
 * Copyright (C) 2010 Jason Garrett-Glaser
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

#include "libavcore/imgutils.h"
#include "avcodec.h"
#include "vp56.h"
#include "vp8data.h"
#include "vp8dsp.h"
#include "h264pred.h"
#include "rectangle.h"

typedef struct {
    uint8_t filter_level;
    uint8_t inner_limit;
    uint8_t inner_filter;
} VP8FilterStrength;

typedef struct {
    uint8_t skip;
    // todo: make it possible to check for at least (i4x4 or split_mv)
    // in one op. are others needed?
    uint8_t mode;
    uint8_t ref_frame;
    uint8_t partitioning;
    VP56mv mv;
    VP56mv bmv[16];
} VP8Macroblock;

typedef struct {
    AVCodecContext *avctx;
    DSPContext dsp;
    VP8DSPContext vp8dsp;
    H264PredContext hpc;
    vp8_mc_func put_pixels_tab[3][3][3];
    AVFrame frames[4];
    AVFrame *framep[4];
    uint8_t *edge_emu_buffer;
    VP56RangeCoder c;   ///< header context, includes mb modes and motion vectors
    int profile;

    int mb_width;   /* number of horizontal MB */
    int mb_height;  /* number of vertical MB */
    int linesize;
    int uvlinesize;

    int keyframe;
    int invisible;
    int update_last;    ///< update VP56_FRAME_PREVIOUS with the current one
    int update_golden;  ///< VP56_FRAME_NONE if not updated, or which frame to copy if so
    int update_altref;
    int deblock_filter;

    /**
     * If this flag is not set, all the probability updates
     * are discarded after this frame is decoded.
     */
    int update_probabilities;

    /**
     * All coefficients are contained in separate arith coding contexts.
     * There can be 1, 2, 4, or 8 of these after the header context.
     */
    int num_coeff_partitions;
    VP56RangeCoder coeff_partition[8];

    VP8Macroblock *macroblocks;
    VP8Macroblock *macroblocks_base;
    VP8FilterStrength *filter_strength;

    uint8_t *intra4x4_pred_mode_top;
    uint8_t intra4x4_pred_mode_left[4];
    uint8_t *segmentation_map;

    /**
     * Cache of the top row needed for intra prediction
     * 16 for luma, 8 for each chroma plane
     */
    uint8_t (*top_border)[16+8+8];

    /**
     * For coeff decode, we need to know whether the above block had non-zero
     * coefficients. This means for each macroblock, we need data for 4 luma
     * blocks, 2 u blocks, 2 v blocks, and the luma dc block, for a total of 9
     * per macroblock. We keep the last row in top_nnz.
     */
    uint8_t (*top_nnz)[9];
    DECLARE_ALIGNED(8, uint8_t, left_nnz)[9];

    /**
     * This is the index plus one of the last non-zero coeff
     * for each of the blocks in the current macroblock.
     * So, 0 -> no coeffs
     *     1 -> dc-only (special transform)
     *     2+-> full transform
     */
    DECLARE_ALIGNED(16, uint8_t, non_zero_count_cache)[6][4];
    DECLARE_ALIGNED(16, DCTELEM, block)[6][4][16];
    DECLARE_ALIGNED(16, DCTELEM, block_dc)[16];
    uint8_t intra4x4_pred_mode_mb[16];

    int chroma_pred_mode;    ///< 8x8c pred mode of the current macroblock
    int segment;             ///< segment of the current macroblock

    int mbskip_enabled;
    int sign_bias[4]; ///< one state [0, 1] per ref frame type
    int ref_count[3];

    /**
     * Base parameters for segmentation, i.e. per-macroblock parameters.
     * These must be kept unchanged even if segmentation is not used for
     * a frame, since the values persist between interframes.
     */
    struct {
        int enabled;
        int absolute_vals;
        int update_map;
        int8_t base_quant[4];
        int8_t filter_level[4];     ///< base loop filter level
    } segmentation;

    /**
     * Macroblocks can have one of 4 different quants in a frame when
     * segmentation is enabled.
     * If segmentation is disabled, only the first segment's values are used.
     */
    struct {
        // [0] - DC qmul  [1] - AC qmul
        int16_t luma_qmul[2];
        int16_t luma_dc_qmul[2];    ///< luma dc-only block quant
        int16_t chroma_qmul[2];
    } qmat[4];

    struct {
        int simple;
        int level;
        int sharpness;
    } filter;

    struct {
        int enabled;    ///< whether each mb can have a different strength based on mode/ref

        /**
         * filter strength adjustment for the following macroblock modes:
         * [0] - i4x4
         * [1] - zero mv
         * [2] - inter modes except for zero or split mv
         * [3] - split mv
         *  i16x16 modes never have any adjustment
         */
        int8_t mode[4];

        /**
         * filter strength adjustment for macroblocks that reference:
         * [0] - intra / VP56_FRAME_CURRENT
         * [1] - VP56_FRAME_PREVIOUS
         * [2] - VP56_FRAME_GOLDEN
         * [3] - altref / VP56_FRAME_GOLDEN2
         */
        int8_t ref[4];
    } lf_delta;

    /**
     * These are all of the updatable probabilities for binary decisions.
     * They are only implictly reset on keyframes, making it quite likely
     * for an interframe to desync if a prior frame's header was corrupt
     * or missing outright!
     */
    struct {
        uint8_t segmentid[3];
        uint8_t mbskip;
        uint8_t intra;
        uint8_t last;
        uint8_t golden;
        uint8_t pred16x16[4];
        uint8_t pred8x8c[3];
        /* Padded to allow overreads */
        uint8_t token[4][17][3][NUM_DCT_TOKENS-1];
        uint8_t mvc[2][19];
    } prob[2];
} VP8Context;

static void vp8_decode_flush(AVCodecContext *avctx)
{
    VP8Context *s = avctx->priv_data;
    int i;

    for (i = 0; i < 4; i++)
        if (s->frames[i].data[0])
            avctx->release_buffer(avctx, &s->frames[i]);
    memset(s->framep, 0, sizeof(s->framep));

    av_freep(&s->macroblocks_base);
    av_freep(&s->filter_strength);
    av_freep(&s->intra4x4_pred_mode_top);
    av_freep(&s->top_nnz);
    av_freep(&s->edge_emu_buffer);
    av_freep(&s->top_border);
    av_freep(&s->segmentation_map);

    s->macroblocks        = NULL;
}

static int update_dimensions(VP8Context *s, int width, int height)
{
    if (av_image_check_size(width, height, 0, s->avctx))
        return AVERROR_INVALIDDATA;

    vp8_decode_flush(s->avctx);

    avcodec_set_dimensions(s->avctx, width, height);

    s->mb_width  = (s->avctx->coded_width +15) / 16;
    s->mb_height = (s->avctx->coded_height+15) / 16;

    s->macroblocks_base        = av_mallocz((s->mb_width+s->mb_height*2+1)*sizeof(*s->macroblocks));
    s->filter_strength         = av_mallocz(s->mb_width*sizeof(*s->filter_strength));
    s->intra4x4_pred_mode_top  = av_mallocz(s->mb_width*4);
    s->top_nnz                 = av_mallocz(s->mb_width*sizeof(*s->top_nnz));
    s->top_border              = av_mallocz((s->mb_width+1)*sizeof(*s->top_border));
    s->segmentation_map        = av_mallocz(s->mb_width*s->mb_height);

    if (!s->macroblocks_base || !s->filter_strength || !s->intra4x4_pred_mode_top ||
        !s->top_nnz || !s->top_border || !s->segmentation_map)
        return AVERROR(ENOMEM);

    s->macroblocks        = s->macroblocks_base + 1;

    return 0;
}

static void parse_segment_info(VP8Context *s)
{
    VP56RangeCoder *c = &s->c;
    int i;

    s->segmentation.update_map = vp8_rac_get(c);

    if (vp8_rac_get(c)) { // update segment feature data
        s->segmentation.absolute_vals = vp8_rac_get(c);

        for (i = 0; i < 4; i++)
            s->segmentation.base_quant[i]   = vp8_rac_get_sint(c, 7);

        for (i = 0; i < 4; i++)
            s->segmentation.filter_level[i] = vp8_rac_get_sint(c, 6);
    }
    if (s->segmentation.update_map)
        for (i = 0; i < 3; i++)
            s->prob->segmentid[i] = vp8_rac_get(c) ? vp8_rac_get_uint(c, 8) : 255;
}

static void update_lf_deltas(VP8Context *s)
{
    VP56RangeCoder *c = &s->c;
    int i;

    for (i = 0; i < 4; i++)
        s->lf_delta.ref[i]  = vp8_rac_get_sint(c, 6);

    for (i = 0; i < 4; i++)
        s->lf_delta.mode[i] = vp8_rac_get_sint(c, 6);
}

static int setup_partitions(VP8Context *s, const uint8_t *buf, int buf_size)
{
    const uint8_t *sizes = buf;
    int i;

    s->num_coeff_partitions = 1 << vp8_rac_get_uint(&s->c, 2);

    buf      += 3*(s->num_coeff_partitions-1);
    buf_size -= 3*(s->num_coeff_partitions-1);
    if (buf_size < 0)
        return -1;

    for (i = 0; i < s->num_coeff_partitions-1; i++) {
        int size = AV_RL24(sizes + 3*i);
        if (buf_size - size < 0)
            return -1;

        ff_vp56_init_range_decoder(&s->coeff_partition[i], buf, size);
        buf      += size;
        buf_size -= size;
    }
    ff_vp56_init_range_decoder(&s->coeff_partition[i], buf, buf_size);

    return 0;
}

static void get_quants(VP8Context *s)
{
    VP56RangeCoder *c = &s->c;
    int i, base_qi;

    int yac_qi     = vp8_rac_get_uint(c, 7);
    int ydc_delta  = vp8_rac_get_sint(c, 4);
    int y2dc_delta = vp8_rac_get_sint(c, 4);
    int y2ac_delta = vp8_rac_get_sint(c, 4);
    int uvdc_delta = vp8_rac_get_sint(c, 4);
    int uvac_delta = vp8_rac_get_sint(c, 4);

    for (i = 0; i < 4; i++) {
        if (s->segmentation.enabled) {
            base_qi = s->segmentation.base_quant[i];
            if (!s->segmentation.absolute_vals)
                base_qi += yac_qi;
        } else
            base_qi = yac_qi;

        s->qmat[i].luma_qmul[0]    =       vp8_dc_qlookup[av_clip(base_qi + ydc_delta , 0, 127)];
        s->qmat[i].luma_qmul[1]    =       vp8_ac_qlookup[av_clip(base_qi             , 0, 127)];
        s->qmat[i].luma_dc_qmul[0] =   2 * vp8_dc_qlookup[av_clip(base_qi + y2dc_delta, 0, 127)];
        s->qmat[i].luma_dc_qmul[1] = 155 * vp8_ac_qlookup[av_clip(base_qi + y2ac_delta, 0, 127)] / 100;
        s->qmat[i].chroma_qmul[0]  =       vp8_dc_qlookup[av_clip(base_qi + uvdc_delta, 0, 127)];
        s->qmat[i].chroma_qmul[1]  =       vp8_ac_qlookup[av_clip(base_qi + uvac_delta, 0, 127)];

        s->qmat[i].luma_dc_qmul[1] = FFMAX(s->qmat[i].luma_dc_qmul[1], 8);
        s->qmat[i].chroma_qmul[0]  = FFMIN(s->qmat[i].chroma_qmul[0], 132);
    }
}

/**
 * Determine which buffers golden and altref should be updated with after this frame.
 * The spec isn't clear here, so I'm going by my understanding of what libvpx does
 *
 * Intra frames update all 3 references
 * Inter frames update VP56_FRAME_PREVIOUS if the update_last flag is set
 * If the update (golden|altref) flag is set, it's updated with the current frame
 *      if update_last is set, and VP56_FRAME_PREVIOUS otherwise.
 * If the flag is not set, the number read means:
 *      0: no update
 *      1: VP56_FRAME_PREVIOUS
 *      2: update golden with altref, or update altref with golden
 */
static VP56Frame ref_to_update(VP8Context *s, int update, VP56Frame ref)
{
    VP56RangeCoder *c = &s->c;

    if (update)
        return VP56_FRAME_CURRENT;

    switch (vp8_rac_get_uint(c, 2)) {
    case 1:
        return VP56_FRAME_PREVIOUS;
    case 2:
        return (ref == VP56_FRAME_GOLDEN) ? VP56_FRAME_GOLDEN2 : VP56_FRAME_GOLDEN;
    }
    return VP56_FRAME_NONE;
}

static void update_refs(VP8Context *s)
{
    VP56RangeCoder *c = &s->c;

    int update_golden = vp8_rac_get(c);
    int update_altref = vp8_rac_get(c);

    s->update_golden = ref_to_update(s, update_golden, VP56_FRAME_GOLDEN);
    s->update_altref = ref_to_update(s, update_altref, VP56_FRAME_GOLDEN2);
}

static int decode_frame_header(VP8Context *s, const uint8_t *buf, int buf_size)
{
    VP56RangeCoder *c = &s->c;
    int header_size, hscale, vscale, i, j, k, l, m, ret;
    int width  = s->avctx->width;
    int height = s->avctx->height;

    s->keyframe  = !(buf[0] & 1);
    s->profile   =  (buf[0]>>1) & 7;
    s->invisible = !(buf[0] & 0x10);
    header_size  = AV_RL24(buf) >> 5;
    buf      += 3;
    buf_size -= 3;

    if (s->profile > 3)
        av_log(s->avctx, AV_LOG_WARNING, "Unknown profile %d\n", s->profile);

    if (!s->profile)
        memcpy(s->put_pixels_tab, s->vp8dsp.put_vp8_epel_pixels_tab, sizeof(s->put_pixels_tab));
    else    // profile 1-3 use bilinear, 4+ aren't defined so whatever
        memcpy(s->put_pixels_tab, s->vp8dsp.put_vp8_bilinear_pixels_tab, sizeof(s->put_pixels_tab));

    if (header_size > buf_size - 7*s->keyframe) {
        av_log(s->avctx, AV_LOG_ERROR, "Header size larger than data provided\n");
        return AVERROR_INVALIDDATA;
    }

    if (s->keyframe) {
        if (AV_RL24(buf) != 0x2a019d) {
            av_log(s->avctx, AV_LOG_ERROR, "Invalid start code 0x%x\n", AV_RL24(buf));
            return AVERROR_INVALIDDATA;
        }
        width  = AV_RL16(buf+3) & 0x3fff;
        height = AV_RL16(buf+5) & 0x3fff;
        hscale = buf[4] >> 6;
        vscale = buf[6] >> 6;
        buf      += 7;
        buf_size -= 7;

        if (hscale || vscale)
            av_log_missing_feature(s->avctx, "Upscaling", 1);

        s->update_golden = s->update_altref = VP56_FRAME_CURRENT;
        for (i = 0; i < 4; i++)
            for (j = 0; j < 16; j++)
                memcpy(s->prob->token[i][j], vp8_token_default_probs[i][vp8_coeff_band[j]],
                       sizeof(s->prob->token[i][j]));
        memcpy(s->prob->pred16x16, vp8_pred16x16_prob_inter, sizeof(s->prob->pred16x16));
        memcpy(s->prob->pred8x8c , vp8_pred8x8c_prob_inter , sizeof(s->prob->pred8x8c));
        memcpy(s->prob->mvc      , vp8_mv_default_prob     , sizeof(s->prob->mvc));
        memset(&s->segmentation, 0, sizeof(s->segmentation));
    }

    if (!s->macroblocks_base || /* first frame */
        width != s->avctx->width || height != s->avctx->height) {
        if ((ret = update_dimensions(s, width, height) < 0))
            return ret;
    }

    ff_vp56_init_range_decoder(c, buf, header_size);
    buf      += header_size;
    buf_size -= header_size;

    if (s->keyframe) {
        if (vp8_rac_get(c))
            av_log(s->avctx, AV_LOG_WARNING, "Unspecified colorspace\n");
        vp8_rac_get(c); // whether we can skip clamping in dsp functions
    }

    if ((s->segmentation.enabled = vp8_rac_get(c)))
        parse_segment_info(s);
    else
        s->segmentation.update_map = 0; // FIXME: move this to some init function?

    s->filter.simple    = vp8_rac_get(c);
    s->filter.level     = vp8_rac_get_uint(c, 6);
    s->filter.sharpness = vp8_rac_get_uint(c, 3);

    if ((s->lf_delta.enabled = vp8_rac_get(c)))
        if (vp8_rac_get(c))
            update_lf_deltas(s);

    if (setup_partitions(s, buf, buf_size)) {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid partitions\n");
        return AVERROR_INVALIDDATA;
    }

    get_quants(s);

    if (!s->keyframe) {
        update_refs(s);
        s->sign_bias[VP56_FRAME_GOLDEN]               = vp8_rac_get(c);
        s->sign_bias[VP56_FRAME_GOLDEN2 /* altref */] = vp8_rac_get(c);
    }

    // if we aren't saving this frame's probabilities for future frames,
    // make a copy of the current probabilities
    if (!(s->update_probabilities = vp8_rac_get(c)))
        s->prob[1] = s->prob[0];

    s->update_last = s->keyframe || vp8_rac_get(c);

    for (i = 0; i < 4; i++)
        for (j = 0; j < 8; j++)
            for (k = 0; k < 3; k++)
                for (l = 0; l < NUM_DCT_TOKENS-1; l++)
                    if (vp56_rac_get_prob_branchy(c, vp8_token_update_probs[i][j][k][l])) {
                        int prob = vp8_rac_get_uint(c, 8);
                        for (m = 0; vp8_coeff_band_indexes[j][m] >= 0; m++)
                            s->prob->token[i][vp8_coeff_band_indexes[j][m]][k][l] = prob;
                    }

    if ((s->mbskip_enabled = vp8_rac_get(c)))
        s->prob->mbskip = vp8_rac_get_uint(c, 8);

    if (!s->keyframe) {
        s->prob->intra  = vp8_rac_get_uint(c, 8);
        s->prob->last   = vp8_rac_get_uint(c, 8);
        s->prob->golden = vp8_rac_get_uint(c, 8);

        if (vp8_rac_get(c))
            for (i = 0; i < 4; i++)
                s->prob->pred16x16[i] = vp8_rac_get_uint(c, 8);
        if (vp8_rac_get(c))
            for (i = 0; i < 3; i++)
                s->prob->pred8x8c[i]  = vp8_rac_get_uint(c, 8);

        // 17.2 MV probability update
        for (i = 0; i < 2; i++)
            for (j = 0; j < 19; j++)
                if (vp56_rac_get_prob_branchy(c, vp8_mv_update_prob[i][j]))
                    s->prob->mvc[i][j] = vp8_rac_get_nn(c);
    }

    return 0;
}

static av_always_inline
void clamp_mv(VP8Context *s, VP56mv *dst, const VP56mv *src, int mb_x, int mb_y)
{
#define MARGIN (16 << 2)
    dst->x = av_clip(src->x, -((mb_x << 6) + MARGIN),
                     ((s->mb_width  - 1 - mb_x) << 6) + MARGIN);
    dst->y = av_clip(src->y, -((mb_y << 6) + MARGIN),
                     ((s->mb_height - 1 - mb_y) << 6) + MARGIN);
}

static av_always_inline
void find_near_mvs(VP8Context *s, VP8Macroblock *mb,
                   VP56mv near[2], VP56mv *best, uint8_t cnt[4])
{
    VP8Macroblock *mb_edge[3] = { mb + 2 /* top */,
                                  mb - 1 /* left */,
                                  mb + 1 /* top-left */ };
    enum { EDGE_TOP, EDGE_LEFT, EDGE_TOPLEFT };
    VP56mv near_mv[4]  = {{ 0 }};
    enum { CNT_ZERO, CNT_NEAREST, CNT_NEAR, CNT_SPLITMV };
    int idx = CNT_ZERO;
    int best_idx = CNT_ZERO;
    int cur_sign_bias = s->sign_bias[mb->ref_frame];
    int *sign_bias = s->sign_bias;

    /* Process MB on top, left and top-left */
    #define MV_EDGE_CHECK(n)\
    {\
        VP8Macroblock *edge = mb_edge[n];\
        int edge_ref = edge->ref_frame;\
        if (edge_ref != VP56_FRAME_CURRENT) {\
            uint32_t mv = AV_RN32A(&edge->mv);\
            if (mv) {\
                if (cur_sign_bias != sign_bias[edge_ref]) {\
                    /* SWAR negate of the values in mv. */\
                    mv = ~mv;\
                    mv = ((mv&0x7fff7fff) + 0x00010001) ^ (mv&0x80008000);\
                }\
                if (!n || mv != AV_RN32A(&near_mv[idx]))\
                    AV_WN32A(&near_mv[++idx], mv);\
                cnt[idx]      += 1 + (n != 2);\
            } else\
                cnt[CNT_ZERO] += 1 + (n != 2);\
        }\
    }
    MV_EDGE_CHECK(0)
    MV_EDGE_CHECK(1)
    MV_EDGE_CHECK(2)

    /* If we have three distinct MVs, merge first and last if they're the same */
    if (cnt[CNT_SPLITMV] && AV_RN32A(&near_mv[1+EDGE_TOP]) == AV_RN32A(&near_mv[1+EDGE_TOPLEFT]))
        cnt[CNT_NEAREST] += 1;

    cnt[CNT_SPLITMV] = ((mb_edge[EDGE_LEFT]->mode   == VP8_MVMODE_SPLIT) +
                        (mb_edge[EDGE_TOP]->mode    == VP8_MVMODE_SPLIT)) * 2 +
                       (mb_edge[EDGE_TOPLEFT]->mode == VP8_MVMODE_SPLIT);

    /* Swap near and nearest if necessary */
    if (cnt[CNT_NEAR] > cnt[CNT_NEAREST]) {
        FFSWAP(uint8_t,     cnt[CNT_NEAREST],     cnt[CNT_NEAR]);
        FFSWAP( VP56mv, near_mv[CNT_NEAREST], near_mv[CNT_NEAR]);
    }

    /* Choose the best mv out of 0,0 and the nearest mv */
    if (cnt[CNT_NEAREST] >= cnt[CNT_ZERO])
        best_idx = CNT_NEAREST;

    mb->mv  = near_mv[best_idx];
    near[0] = near_mv[CNT_NEAREST];
    near[1] = near_mv[CNT_NEAR];
}

/**
 * Motion vector coding, 17.1.
 */
static int read_mv_component(VP56RangeCoder *c, const uint8_t *p)
{
    int bit, x = 0;

    if (vp56_rac_get_prob_branchy(c, p[0])) {
        int i;

        for (i = 0; i < 3; i++)
            x += vp56_rac_get_prob(c, p[9 + i]) << i;
        for (i = 9; i > 3; i--)
            x += vp56_rac_get_prob(c, p[9 + i]) << i;
        if (!(x & 0xFFF0) || vp56_rac_get_prob(c, p[12]))
            x += 8;
    } else {
        // small_mvtree
        const uint8_t *ps = p+2;
        bit = vp56_rac_get_prob(c, *ps);
        ps += 1 + 3*bit;
        x  += 4*bit;
        bit = vp56_rac_get_prob(c, *ps);
        ps += 1 + bit;
        x  += 2*bit;
        x  += vp56_rac_get_prob(c, *ps);
    }

    return (x && vp56_rac_get_prob(c, p[1])) ? -x : x;
}

static av_always_inline
const uint8_t *get_submv_prob(uint32_t left, uint32_t top)
{
    if (left == top)
        return vp8_submv_prob[4-!!left];
    if (!top)
        return vp8_submv_prob[2];
    return vp8_submv_prob[1-!!left];
}

/**
 * Split motion vector prediction, 16.4.
 * @returns the number of motion vectors parsed (2, 4 or 16)
 */
static av_always_inline
int decode_splitmvs(VP8Context *s, VP56RangeCoder *c, VP8Macroblock *mb)
{
    int part_idx;
    int n, num;
    VP8Macroblock *top_mb  = &mb[2];
    VP8Macroblock *left_mb = &mb[-1];
    const uint8_t *mbsplits_left = vp8_mbsplits[left_mb->partitioning],
                  *mbsplits_top = vp8_mbsplits[top_mb->partitioning],
                  *mbsplits_cur, *firstidx;
    VP56mv *top_mv  = top_mb->bmv;
    VP56mv *left_mv = left_mb->bmv;
    VP56mv *cur_mv  = mb->bmv;

    if (vp56_rac_get_prob_branchy(c, vp8_mbsplit_prob[0])) {
        if (vp56_rac_get_prob_branchy(c, vp8_mbsplit_prob[1])) {
            part_idx = VP8_SPLITMVMODE_16x8 + vp56_rac_get_prob(c, vp8_mbsplit_prob[2]);
        } else {
            part_idx = VP8_SPLITMVMODE_8x8;
        }
    } else {
        part_idx = VP8_SPLITMVMODE_4x4;
    }

    num = vp8_mbsplit_count[part_idx];
    mbsplits_cur = vp8_mbsplits[part_idx],
    firstidx = vp8_mbfirstidx[part_idx];
    mb->partitioning = part_idx;

    for (n = 0; n < num; n++) {
        int k = firstidx[n];
        uint32_t left, above;
        const uint8_t *submv_prob;

        if (!(k & 3))
            left = AV_RN32A(&left_mv[mbsplits_left[k + 3]]);
        else
            left  = AV_RN32A(&cur_mv[mbsplits_cur[k - 1]]);
        if (k <= 3)
            above = AV_RN32A(&top_mv[mbsplits_top[k + 12]]);
        else
            above = AV_RN32A(&cur_mv[mbsplits_cur[k - 4]]);

        submv_prob = get_submv_prob(left, above);

        if (vp56_rac_get_prob_branchy(c, submv_prob[0])) {
            if (vp56_rac_get_prob_branchy(c, submv_prob[1])) {
                if (vp56_rac_get_prob_branchy(c, submv_prob[2])) {
                    mb->bmv[n].y = mb->mv.y + read_mv_component(c, s->prob->mvc[0]);
                    mb->bmv[n].x = mb->mv.x + read_mv_component(c, s->prob->mvc[1]);
                } else {
                    AV_ZERO32(&mb->bmv[n]);
                }
            } else {
                AV_WN32A(&mb->bmv[n], above);
            }
        } else {
            AV_WN32A(&mb->bmv[n], left);
        }
    }

    return num;
}

static av_always_inline
void decode_intra4x4_modes(VP8Context *s, VP56RangeCoder *c,
                           int mb_x, int keyframe)
{
    uint8_t *intra4x4 = s->intra4x4_pred_mode_mb;
    if (keyframe) {
        int x, y;
        uint8_t* const top = s->intra4x4_pred_mode_top + 4 * mb_x;
        uint8_t* const left = s->intra4x4_pred_mode_left;
        for (y = 0; y < 4; y++) {
            for (x = 0; x < 4; x++) {
                const uint8_t *ctx;
                ctx = vp8_pred4x4_prob_intra[top[x]][left[y]];
                *intra4x4 = vp8_rac_get_tree(c, vp8_pred4x4_tree, ctx);
                left[y] = top[x] = *intra4x4;
                intra4x4++;
            }
        }
    } else {
        int i;
        for (i = 0; i < 16; i++)
            intra4x4[i] = vp8_rac_get_tree(c, vp8_pred4x4_tree, vp8_pred4x4_prob_inter);
    }
}

static av_always_inline
void decode_mb_mode(VP8Context *s, VP8Macroblock *mb, int mb_x, int mb_y, uint8_t *segment)
{
    VP56RangeCoder *c = &s->c;

    if (s->segmentation.update_map)
        *segment = vp8_rac_get_tree(c, vp8_segmentid_tree, s->prob->segmentid);
    s->segment = *segment;

    mb->skip = s->mbskip_enabled ? vp56_rac_get_prob(c, s->prob->mbskip) : 0;

    if (s->keyframe) {
        mb->mode = vp8_rac_get_tree(c, vp8_pred16x16_tree_intra, vp8_pred16x16_prob_intra);

        if (mb->mode == MODE_I4x4) {
            decode_intra4x4_modes(s, c, mb_x, 1);
        } else {
            const uint32_t modes = vp8_pred4x4_mode[mb->mode] * 0x01010101u;
            AV_WN32A(s->intra4x4_pred_mode_top + 4 * mb_x, modes);
            AV_WN32A(s->intra4x4_pred_mode_left, modes);
        }

        s->chroma_pred_mode = vp8_rac_get_tree(c, vp8_pred8x8c_tree, vp8_pred8x8c_prob_intra);
        mb->ref_frame = VP56_FRAME_CURRENT;
    } else if (vp56_rac_get_prob_branchy(c, s->prob->intra)) {
        VP56mv near[2], best;
        uint8_t cnt[4] = { 0 };

        // inter MB, 16.2
        if (vp56_rac_get_prob_branchy(c, s->prob->last))
            mb->ref_frame = vp56_rac_get_prob(c, s->prob->golden) ?
                VP56_FRAME_GOLDEN2 /* altref */ : VP56_FRAME_GOLDEN;
        else
            mb->ref_frame = VP56_FRAME_PREVIOUS;
        s->ref_count[mb->ref_frame-1]++;

        // motion vectors, 16.3
        find_near_mvs(s, mb, near, &best, cnt);
        if (vp56_rac_get_prob_branchy(c, vp8_mode_contexts[cnt[0]][0])) {
            if (vp56_rac_get_prob_branchy(c, vp8_mode_contexts[cnt[1]][1])) {
                if (vp56_rac_get_prob_branchy(c, vp8_mode_contexts[cnt[2]][2])) {
                    if (vp56_rac_get_prob_branchy(c, vp8_mode_contexts[cnt[3]][3])) {
                        mb->mode = VP8_MVMODE_SPLIT;
                        clamp_mv(s, &mb->mv, &mb->mv, mb_x, mb_y);
                        mb->mv = mb->bmv[decode_splitmvs(s, c, mb) - 1];
                    } else {
                        mb->mode = VP8_MVMODE_NEW;
                        clamp_mv(s, &mb->mv, &mb->mv, mb_x, mb_y);
                        mb->mv.y += read_mv_component(c, s->prob->mvc[0]);
                        mb->mv.x += read_mv_component(c, s->prob->mvc[1]);
                    }
                } else {
                    mb->mode = VP8_MVMODE_NEAR;
                    clamp_mv(s, &mb->mv, &near[1], mb_x, mb_y);
                }
            } else {
                mb->mode = VP8_MVMODE_NEAREST;
                clamp_mv(s, &mb->mv, &near[0], mb_x, mb_y);
            }
        } else {
            mb->mode = VP8_MVMODE_ZERO;
            AV_ZERO32(&mb->mv);
        }
        if (mb->mode != VP8_MVMODE_SPLIT) {
            mb->partitioning = VP8_SPLITMVMODE_NONE;
            mb->bmv[0] = mb->mv;
        }
    } else {
        // intra MB, 16.1
        mb->mode = vp8_rac_get_tree(c, vp8_pred16x16_tree_inter, s->prob->pred16x16);

        if (mb->mode == MODE_I4x4)
            decode_intra4x4_modes(s, c, mb_x, 0);

        s->chroma_pred_mode = vp8_rac_get_tree(c, vp8_pred8x8c_tree, s->prob->pred8x8c);
        mb->ref_frame = VP56_FRAME_CURRENT;
        mb->partitioning = VP8_SPLITMVMODE_NONE;
        AV_ZERO32(&mb->bmv[0]);
    }
}

/**
 * @param c arithmetic bitstream reader context
 * @param block destination for block coefficients
 * @param probs probabilities to use when reading trees from the bitstream
 * @param i initial coeff index, 0 unless a separate DC block is coded
 * @param zero_nhood the initial prediction context for number of surrounding
 *                   all-zero blocks (only left/top, so 0-2)
 * @param qmul array holding the dc/ac dequant factor at position 0/1
 * @return 0 if no coeffs were decoded
 *         otherwise, the index of the last coeff decoded plus one
 */
static int decode_block_coeffs_internal(VP56RangeCoder *c, DCTELEM block[16],
                                        uint8_t probs[8][3][NUM_DCT_TOKENS-1],
                                        int i, uint8_t *token_prob, int16_t qmul[2])
{
    goto skip_eob;
    do {
        int coeff;
        if (!vp56_rac_get_prob_branchy(c, token_prob[0]))   // DCT_EOB
            return i;

skip_eob:
        if (!vp56_rac_get_prob_branchy(c, token_prob[1])) { // DCT_0
            if (++i == 16)
                return i; // invalid input; blocks should end with EOB
            token_prob = probs[i][0];
            goto skip_eob;
        }

        if (!vp56_rac_get_prob_branchy(c, token_prob[2])) { // DCT_1
            coeff = 1;
            token_prob = probs[i+1][1];
        } else {
            if (!vp56_rac_get_prob_branchy(c, token_prob[3])) { // DCT 2,3,4
                coeff = vp56_rac_get_prob_branchy(c, token_prob[4]);
                if (coeff)
                    coeff += vp56_rac_get_prob(c, token_prob[5]);
                coeff += 2;
            } else {
                // DCT_CAT*
                if (!vp56_rac_get_prob_branchy(c, token_prob[6])) {
                    if (!vp56_rac_get_prob_branchy(c, token_prob[7])) { // DCT_CAT1
                        coeff  = 5 + vp56_rac_get_prob(c, vp8_dct_cat1_prob[0]);
                    } else {                                    // DCT_CAT2
                        coeff  = 7;
                        coeff += vp56_rac_get_prob(c, vp8_dct_cat2_prob[0]) << 1;
                        coeff += vp56_rac_get_prob(c, vp8_dct_cat2_prob[1]);
                    }
                } else {    // DCT_CAT3 and up
                    int a = vp56_rac_get_prob(c, token_prob[8]);
                    int b = vp56_rac_get_prob(c, token_prob[9+a]);
                    int cat = (a<<1) + b;
                    coeff  = 3 + (8<<cat);
                    coeff += vp8_rac_get_coeff(c, vp8_dct_cat_prob[cat]);
                }
            }
            token_prob = probs[i+1][2];
        }
        block[zigzag_scan[i]] = (vp8_rac_get(c) ? -coeff : coeff) * qmul[!!i];
    } while (++i < 16);

    return i;
}

static av_always_inline
int decode_block_coeffs(VP56RangeCoder *c, DCTELEM block[16],
                        uint8_t probs[8][3][NUM_DCT_TOKENS-1],
                        int i, int zero_nhood, int16_t qmul[2])
{
    uint8_t *token_prob = probs[i][zero_nhood];
    if (!vp56_rac_get_prob_branchy(c, token_prob[0]))   // DCT_EOB
        return 0;
    return decode_block_coeffs_internal(c, block, probs, i, token_prob, qmul);
}

static av_always_inline
void decode_mb_coeffs(VP8Context *s, VP56RangeCoder *c, VP8Macroblock *mb,
                      uint8_t t_nnz[9], uint8_t l_nnz[9])
{
    int i, x, y, luma_start = 0, luma_ctx = 3;
    int nnz_pred, nnz, nnz_total = 0;
    int segment = s->segment;
    int block_dc = 0;

    if (mb->mode != MODE_I4x4 && mb->mode != VP8_MVMODE_SPLIT) {
        nnz_pred = t_nnz[8] + l_nnz[8];

        // decode DC values and do hadamard
        nnz = decode_block_coeffs(c, s->block_dc, s->prob->token[1], 0, nnz_pred,
                                  s->qmat[segment].luma_dc_qmul);
        l_nnz[8] = t_nnz[8] = !!nnz;
        if (nnz) {
            nnz_total += nnz;
            block_dc = 1;
            if (nnz == 1)
                s->vp8dsp.vp8_luma_dc_wht_dc(s->block, s->block_dc);
            else
                s->vp8dsp.vp8_luma_dc_wht(s->block, s->block_dc);
        }
        luma_start = 1;
        luma_ctx = 0;
    }

    // luma blocks
    for (y = 0; y < 4; y++)
        for (x = 0; x < 4; x++) {
            nnz_pred = l_nnz[y] + t_nnz[x];
            nnz = decode_block_coeffs(c, s->block[y][x], s->prob->token[luma_ctx], luma_start,
                                      nnz_pred, s->qmat[segment].luma_qmul);
            // nnz+block_dc may be one more than the actual last index, but we don't care
            s->non_zero_count_cache[y][x] = nnz + block_dc;
            t_nnz[x] = l_nnz[y] = !!nnz;
            nnz_total += nnz;
        }

    // chroma blocks
    // TODO: what to do about dimensions? 2nd dim for luma is x,
    // but for chroma it's (y<<1)|x
    for (i = 4; i < 6; i++)
        for (y = 0; y < 2; y++)
            for (x = 0; x < 2; x++) {
                nnz_pred = l_nnz[i+2*y] + t_nnz[i+2*x];
                nnz = decode_block_coeffs(c, s->block[i][(y<<1)+x], s->prob->token[2], 0,
                                          nnz_pred, s->qmat[segment].chroma_qmul);
                s->non_zero_count_cache[i][(y<<1)+x] = nnz;
                t_nnz[i+2*x] = l_nnz[i+2*y] = !!nnz;
                nnz_total += nnz;
            }

    // if there were no coded coeffs despite the macroblock not being marked skip,
    // we MUST not do the inner loop filter and should not do IDCT
    // Since skip isn't used for bitstream prediction, just manually set it.
    if (!nnz_total)
        mb->skip = 1;
}

static av_always_inline
void backup_mb_border(uint8_t *top_border, uint8_t *src_y, uint8_t *src_cb, uint8_t *src_cr,
                      int linesize, int uvlinesize, int simple)
{
    AV_COPY128(top_border, src_y + 15*linesize);
    if (!simple) {
        AV_COPY64(top_border+16, src_cb + 7*uvlinesize);
        AV_COPY64(top_border+24, src_cr + 7*uvlinesize);
    }
}

static av_always_inline
void xchg_mb_border(uint8_t *top_border, uint8_t *src_y, uint8_t *src_cb, uint8_t *src_cr,
                    int linesize, int uvlinesize, int mb_x, int mb_y, int mb_width,
                    int simple, int xchg)
{
    uint8_t *top_border_m1 = top_border-32;     // for TL prediction
    src_y  -=   linesize;
    src_cb -= uvlinesize;
    src_cr -= uvlinesize;

#define XCHG(a,b,xchg) do {                     \
        if (xchg) AV_SWAP64(b,a);               \
        else      AV_COPY64(b,a);               \
    } while (0)

    XCHG(top_border_m1+8, src_y-8, xchg);
    XCHG(top_border,      src_y,   xchg);
    XCHG(top_border+8,    src_y+8, 1);
    if (mb_x < mb_width-1)
        XCHG(top_border+32, src_y+16, 1);

    // only copy chroma for normal loop filter
    // or to initialize the top row to 127
    if (!simple || !mb_y) {
        XCHG(top_border_m1+16, src_cb-8, xchg);
        XCHG(top_border_m1+24, src_cr-8, xchg);
        XCHG(top_border+16,    src_cb, 1);
        XCHG(top_border+24,    src_cr, 1);
    }
}

static av_always_inline
int check_intra_pred_mode(int mode, int mb_x, int mb_y)
{
    if (mode == DC_PRED8x8) {
        if (!mb_x) {
            mode = mb_y ? TOP_DC_PRED8x8 : DC_128_PRED8x8;
        } else if (!mb_y) {
            mode = LEFT_DC_PRED8x8;
        }
    }
    return mode;
}

static av_always_inline
void intra_predict(VP8Context *s, uint8_t *dst[3], VP8Macroblock *mb,
                   int mb_x, int mb_y)
{
    int x, y, mode, nnz, tr;

    // for the first row, we need to run xchg_mb_border to init the top edge to 127
    // otherwise, skip it if we aren't going to deblock
    if (s->deblock_filter || !mb_y)
        xchg_mb_border(s->top_border[mb_x+1], dst[0], dst[1], dst[2],
                       s->linesize, s->uvlinesize, mb_x, mb_y, s->mb_width,
                       s->filter.simple, 1);

    if (mb->mode < MODE_I4x4) {
        mode = check_intra_pred_mode(mb->mode, mb_x, mb_y);
        s->hpc.pred16x16[mode](dst[0], s->linesize);
    } else {
        uint8_t *ptr = dst[0];
        uint8_t *intra4x4 = s->intra4x4_pred_mode_mb;

        // all blocks on the right edge of the macroblock use bottom edge
        // the top macroblock for their topright edge
        uint8_t *tr_right = ptr - s->linesize + 16;

        // if we're on the right edge of the frame, said edge is extended
        // from the top macroblock
        if (mb_x == s->mb_width-1) {
            tr = tr_right[-1]*0x01010101;
            tr_right = (uint8_t *)&tr;
        }

        if (mb->skip)
            AV_ZERO128(s->non_zero_count_cache);

        for (y = 0; y < 4; y++) {
            uint8_t *topright = ptr + 4 - s->linesize;
            for (x = 0; x < 4; x++) {
                if (x == 3)
                    topright = tr_right;

                s->hpc.pred4x4[intra4x4[x]](ptr+4*x, topright, s->linesize);

                nnz = s->non_zero_count_cache[y][x];
                if (nnz) {
                    if (nnz == 1)
                        s->vp8dsp.vp8_idct_dc_add(ptr+4*x, s->block[y][x], s->linesize);
                    else
                        s->vp8dsp.vp8_idct_add(ptr+4*x, s->block[y][x], s->linesize);
                }
                topright += 4;
            }

            ptr   += 4*s->linesize;
            intra4x4 += 4;
        }
    }

    mode = check_intra_pred_mode(s->chroma_pred_mode, mb_x, mb_y);
    s->hpc.pred8x8[mode](dst[1], s->uvlinesize);
    s->hpc.pred8x8[mode](dst[2], s->uvlinesize);

    if (s->deblock_filter || !mb_y)
        xchg_mb_border(s->top_border[mb_x+1], dst[0], dst[1], dst[2],
                       s->linesize, s->uvlinesize, mb_x, mb_y, s->mb_width,
                       s->filter.simple, 0);
}

/**
 * Generic MC function.
 *
 * @param s VP8 decoding context
 * @param luma 1 for luma (Y) planes, 0 for chroma (Cb/Cr) planes
 * @param dst target buffer for block data at block position
 * @param src reference picture buffer at origin (0, 0)
 * @param mv motion vector (relative to block position) to get pixel data from
 * @param x_off horizontal position of block from origin (0, 0)
 * @param y_off vertical position of block from origin (0, 0)
 * @param block_w width of block (16, 8 or 4)
 * @param block_h height of block (always same as block_w)
 * @param width width of src/dst plane data
 * @param height height of src/dst plane data
 * @param linesize size of a single line of plane data, including padding
 * @param mc_func motion compensation function pointers (bilinear or sixtap MC)
 */
static av_always_inline
void vp8_mc(VP8Context *s, int luma,
            uint8_t *dst, uint8_t *src, const VP56mv *mv,
            int x_off, int y_off, int block_w, int block_h,
            int width, int height, int linesize,
            vp8_mc_func mc_func[3][3])
{
    if (AV_RN32A(mv)) {
        static const uint8_t idx[8] = { 0, 1, 2, 1, 2, 1, 2, 1 };
        int mx = (mv->x << luma)&7, mx_idx = idx[mx];
        int my = (mv->y << luma)&7, my_idx = idx[my];

        x_off += mv->x >> (3 - luma);
        y_off += mv->y >> (3 - luma);

        // edge emulation
        src += y_off * linesize + x_off;
        if (x_off < 2 || x_off >= width  - block_w - 3 ||
            y_off < 2 || y_off >= height - block_h - 3) {
            ff_emulated_edge_mc(s->edge_emu_buffer, src - 2 * linesize - 2, linesize,
                                block_w + 5, block_h + 5,
                                x_off - 2, y_off - 2, width, height);
            src = s->edge_emu_buffer + 2 + linesize * 2;
        }
        mc_func[my_idx][mx_idx](dst, linesize, src, linesize, block_h, mx, my);
    } else
        mc_func[0][0](dst, linesize, src + y_off * linesize + x_off, linesize, block_h, 0, 0);
}

static av_always_inline
void vp8_mc_part(VP8Context *s, uint8_t *dst[3],
                 AVFrame *ref_frame, int x_off, int y_off,
                 int bx_off, int by_off,
                 int block_w, int block_h,
                 int width, int height, VP56mv *mv)
{
    VP56mv uvmv = *mv;

    /* Y */
    vp8_mc(s, 1, dst[0] + by_off * s->linesize + bx_off,
           ref_frame->data[0], mv, x_off + bx_off, y_off + by_off,
           block_w, block_h, width, height, s->linesize,
           s->put_pixels_tab[block_w == 8]);

    /* U/V */
    if (s->profile == 3) {
        uvmv.x &= ~7;
        uvmv.y &= ~7;
    }
    x_off   >>= 1; y_off   >>= 1;
    bx_off  >>= 1; by_off  >>= 1;
    width   >>= 1; height  >>= 1;
    block_w >>= 1; block_h >>= 1;
    vp8_mc(s, 0, dst[1] + by_off * s->uvlinesize + bx_off,
           ref_frame->data[1], &uvmv, x_off + bx_off, y_off + by_off,
           block_w, block_h, width, height, s->uvlinesize,
           s->put_pixels_tab[1 + (block_w == 4)]);
    vp8_mc(s, 0, dst[2] + by_off * s->uvlinesize + bx_off,
           ref_frame->data[2], &uvmv, x_off + bx_off, y_off + by_off,
           block_w, block_h, width, height, s->uvlinesize,
           s->put_pixels_tab[1 + (block_w == 4)]);
}

/* Fetch pixels for estimated mv 4 macroblocks ahead.
 * Optimized for 64-byte cache lines.  Inspired by ffh264 prefetch_motion. */
static av_always_inline void prefetch_motion(VP8Context *s, VP8Macroblock *mb, int mb_x, int mb_y, int mb_xy, int ref)
{
    /* Don't prefetch refs that haven't been used very often this frame. */
    if (s->ref_count[ref-1] > (mb_xy >> 5)) {
        int x_off = mb_x << 4, y_off = mb_y << 4;
        int mx = (mb->mv.x>>2) + x_off + 8;
        int my = (mb->mv.y>>2) + y_off;
        uint8_t **src= s->framep[ref]->data;
        int off= mx + (my + (mb_x&3)*4)*s->linesize + 64;
        s->dsp.prefetch(src[0]+off, s->linesize, 4);
        off= (mx>>1) + ((my>>1) + (mb_x&7))*s->uvlinesize + 64;
        s->dsp.prefetch(src[1]+off, src[2]-src[1], 2);
    }
}

/**
 * Apply motion vectors to prediction buffer, chapter 18.
 */
static av_always_inline
void inter_predict(VP8Context *s, uint8_t *dst[3], VP8Macroblock *mb,
                   int mb_x, int mb_y)
{
    int x_off = mb_x << 4, y_off = mb_y << 4;
    int width = 16*s->mb_width, height = 16*s->mb_height;
    AVFrame *ref = s->framep[mb->ref_frame];
    VP56mv *bmv = mb->bmv;

    if (mb->mode < VP8_MVMODE_SPLIT) {
        vp8_mc_part(s, dst, ref, x_off, y_off,
                    0, 0, 16, 16, width, height, &mb->mv);
    } else switch (mb->partitioning) {
    case VP8_SPLITMVMODE_4x4: {
        int x, y;
        VP56mv uvmv;

        /* Y */
        for (y = 0; y < 4; y++) {
            for (x = 0; x < 4; x++) {
                vp8_mc(s, 1, dst[0] + 4*y*s->linesize + x*4,
                       ref->data[0], &bmv[4*y + x],
                       4*x + x_off, 4*y + y_off, 4, 4,
                       width, height, s->linesize,
                       s->put_pixels_tab[2]);
            }
        }

        /* U/V */
        x_off >>= 1; y_off >>= 1; width >>= 1; height >>= 1;
        for (y = 0; y < 2; y++) {
            for (x = 0; x < 2; x++) {
                uvmv.x = mb->bmv[ 2*y    * 4 + 2*x  ].x +
                         mb->bmv[ 2*y    * 4 + 2*x+1].x +
                         mb->bmv[(2*y+1) * 4 + 2*x  ].x +
                         mb->bmv[(2*y+1) * 4 + 2*x+1].x;
                uvmv.y = mb->bmv[ 2*y    * 4 + 2*x  ].y +
                         mb->bmv[ 2*y    * 4 + 2*x+1].y +
                         mb->bmv[(2*y+1) * 4 + 2*x  ].y +
                         mb->bmv[(2*y+1) * 4 + 2*x+1].y;
                uvmv.x = (uvmv.x + 2 + (uvmv.x >> (INT_BIT-1))) >> 2;
                uvmv.y = (uvmv.y + 2 + (uvmv.y >> (INT_BIT-1))) >> 2;
                if (s->profile == 3) {
                    uvmv.x &= ~7;
                    uvmv.y &= ~7;
                }
                vp8_mc(s, 0, dst[1] + 4*y*s->uvlinesize + x*4,
                       ref->data[1], &uvmv,
                       4*x + x_off, 4*y + y_off, 4, 4,
                       width, height, s->uvlinesize,
                       s->put_pixels_tab[2]);
                vp8_mc(s, 0, dst[2] + 4*y*s->uvlinesize + x*4,
                       ref->data[2], &uvmv,
                       4*x + x_off, 4*y + y_off, 4, 4,
                       width, height, s->uvlinesize,
                       s->put_pixels_tab[2]);
            }
        }
        break;
    }
    case VP8_SPLITMVMODE_16x8:
        vp8_mc_part(s, dst, ref, x_off, y_off,
                    0, 0, 16, 8, width, height, &bmv[0]);
        vp8_mc_part(s, dst, ref, x_off, y_off,
                    0, 8, 16, 8, width, height, &bmv[1]);
        break;
    case VP8_SPLITMVMODE_8x16:
        vp8_mc_part(s, dst, ref, x_off, y_off,
                    0, 0, 8, 16, width, height, &bmv[0]);
        vp8_mc_part(s, dst, ref, x_off, y_off,
                    8, 0, 8, 16, width, height, &bmv[1]);
        break;
    case VP8_SPLITMVMODE_8x8:
        vp8_mc_part(s, dst, ref, x_off, y_off,
                    0, 0, 8, 8, width, height, &bmv[0]);
        vp8_mc_part(s, dst, ref, x_off, y_off,
                    8, 0, 8, 8, width, height, &bmv[1]);
        vp8_mc_part(s, dst, ref, x_off, y_off,
                    0, 8, 8, 8, width, height, &bmv[2]);
        vp8_mc_part(s, dst, ref, x_off, y_off,
                    8, 8, 8, 8, width, height, &bmv[3]);
        break;
    }
}

static av_always_inline void idct_mb(VP8Context *s, uint8_t *dst[3], VP8Macroblock *mb)
{
    int x, y, ch;

    if (mb->mode != MODE_I4x4) {
        uint8_t *y_dst = dst[0];
        for (y = 0; y < 4; y++) {
            uint32_t nnz4 = AV_RN32A(s->non_zero_count_cache[y]);
            if (nnz4) {
                if (nnz4&~0x01010101) {
                    for (x = 0; x < 4; x++) {
                        int nnz = s->non_zero_count_cache[y][x];
                        if (nnz) {
                            if (nnz == 1)
                                s->vp8dsp.vp8_idct_dc_add(y_dst+4*x, s->block[y][x], s->linesize);
                            else
                                s->vp8dsp.vp8_idct_add(y_dst+4*x, s->block[y][x], s->linesize);
                        }
                    }
                } else {
                    s->vp8dsp.vp8_idct_dc_add4y(y_dst, s->block[y], s->linesize);
                }
            }
            y_dst += 4*s->linesize;
        }
    }

    for (ch = 0; ch < 2; ch++) {
        uint32_t nnz4 = AV_RN32A(s->non_zero_count_cache[4+ch]);
        if (nnz4) {
            uint8_t *ch_dst = dst[1+ch];
            if (nnz4&~0x01010101) {
                for (y = 0; y < 2; y++) {
                    for (x = 0; x < 2; x++) {
                        int nnz = s->non_zero_count_cache[4+ch][(y<<1)+x];
                        if (nnz) {
                            if (nnz == 1)
                                s->vp8dsp.vp8_idct_dc_add(ch_dst+4*x, s->block[4+ch][(y<<1)+x], s->uvlinesize);
                            else
                                s->vp8dsp.vp8_idct_add(ch_dst+4*x, s->block[4+ch][(y<<1)+x], s->uvlinesize);
                        }
                    }
                    ch_dst += 4*s->uvlinesize;
                }
            } else {
                s->vp8dsp.vp8_idct_dc_add4uv(ch_dst, s->block[4+ch], s->uvlinesize);
            }
        }
    }
}

static av_always_inline void filter_level_for_mb(VP8Context *s, VP8Macroblock *mb, VP8FilterStrength *f )
{
    int interior_limit, filter_level;

    if (s->segmentation.enabled) {
        filter_level = s->segmentation.filter_level[s->segment];
        if (!s->segmentation.absolute_vals)
            filter_level += s->filter.level;
    } else
        filter_level = s->filter.level;

    if (s->lf_delta.enabled) {
        filter_level += s->lf_delta.ref[mb->ref_frame];

        if (mb->ref_frame == VP56_FRAME_CURRENT) {
            if (mb->mode == MODE_I4x4)
                filter_level += s->lf_delta.mode[0];
        } else {
            if (mb->mode == VP8_MVMODE_ZERO)
                filter_level += s->lf_delta.mode[1];
            else if (mb->mode == VP8_MVMODE_SPLIT)
                filter_level += s->lf_delta.mode[3];
            else
                filter_level += s->lf_delta.mode[2];
        }
    }
    filter_level = av_clip(filter_level, 0, 63);

    interior_limit = filter_level;
    if (s->filter.sharpness) {
        interior_limit >>= s->filter.sharpness > 4 ? 2 : 1;
        interior_limit = FFMIN(interior_limit, 9 - s->filter.sharpness);
    }
    interior_limit = FFMAX(interior_limit, 1);

    f->filter_level = filter_level;
    f->inner_limit = interior_limit;
    f->inner_filter = !mb->skip || mb->mode == MODE_I4x4 || mb->mode == VP8_MVMODE_SPLIT;
}

static av_always_inline void filter_mb(VP8Context *s, uint8_t *dst[3], VP8FilterStrength *f, int mb_x, int mb_y)
{
    int mbedge_lim, bedge_lim, hev_thresh;
    int filter_level = f->filter_level;
    int inner_limit = f->inner_limit;
    int inner_filter = f->inner_filter;
    int linesize = s->linesize;
    int uvlinesize = s->uvlinesize;

    if (!filter_level)
        return;

    mbedge_lim = 2*(filter_level+2) + inner_limit;
     bedge_lim = 2* filter_level    + inner_limit;
    hev_thresh = filter_level >= 15;

    if (s->keyframe) {
        if (filter_level >= 40)
            hev_thresh = 2;
    } else {
        if (filter_level >= 40)
            hev_thresh = 3;
        else if (filter_level >= 20)
            hev_thresh = 2;
    }

    if (mb_x) {
        s->vp8dsp.vp8_h_loop_filter16y(dst[0],     linesize,
                                       mbedge_lim, inner_limit, hev_thresh);
        s->vp8dsp.vp8_h_loop_filter8uv(dst[1],     dst[2],      uvlinesize,
                                       mbedge_lim, inner_limit, hev_thresh);
    }

    if (inner_filter) {
        s->vp8dsp.vp8_h_loop_filter16y_inner(dst[0]+ 4, linesize, bedge_lim,
                                             inner_limit, hev_thresh);
        s->vp8dsp.vp8_h_loop_filter16y_inner(dst[0]+ 8, linesize, bedge_lim,
                                             inner_limit, hev_thresh);
        s->vp8dsp.vp8_h_loop_filter16y_inner(dst[0]+12, linesize, bedge_lim,
                                             inner_limit, hev_thresh);
        s->vp8dsp.vp8_h_loop_filter8uv_inner(dst[1] + 4, dst[2] + 4,
                                             uvlinesize,  bedge_lim,
                                             inner_limit, hev_thresh);
    }

    if (mb_y) {
        s->vp8dsp.vp8_v_loop_filter16y(dst[0],     linesize,
                                       mbedge_lim, inner_limit, hev_thresh);
        s->vp8dsp.vp8_v_loop_filter8uv(dst[1],     dst[2],      uvlinesize,
                                       mbedge_lim, inner_limit, hev_thresh);
    }

    if (inner_filter) {
        s->vp8dsp.vp8_v_loop_filter16y_inner(dst[0]+ 4*linesize,
                                             linesize,    bedge_lim,
                                             inner_limit, hev_thresh);
        s->vp8dsp.vp8_v_loop_filter16y_inner(dst[0]+ 8*linesize,
                                             linesize,    bedge_lim,
                                             inner_limit, hev_thresh);
        s->vp8dsp.vp8_v_loop_filter16y_inner(dst[0]+12*linesize,
                                             linesize,    bedge_lim,
                                             inner_limit, hev_thresh);
        s->vp8dsp.vp8_v_loop_filter8uv_inner(dst[1] + 4 * uvlinesize,
                                             dst[2] + 4 * uvlinesize,
                                             uvlinesize,  bedge_lim,
                                             inner_limit, hev_thresh);
    }
}

static av_always_inline void filter_mb_simple(VP8Context *s, uint8_t *dst, VP8FilterStrength *f, int mb_x, int mb_y)
{
    int mbedge_lim, bedge_lim;
    int filter_level = f->filter_level;
    int inner_limit = f->inner_limit;
    int inner_filter = f->inner_filter;
    int linesize = s->linesize;

    if (!filter_level)
        return;

    mbedge_lim = 2*(filter_level+2) + inner_limit;
     bedge_lim = 2* filter_level    + inner_limit;

    if (mb_x)
        s->vp8dsp.vp8_h_loop_filter_simple(dst, linesize, mbedge_lim);
    if (inner_filter) {
        s->vp8dsp.vp8_h_loop_filter_simple(dst+ 4, linesize, bedge_lim);
        s->vp8dsp.vp8_h_loop_filter_simple(dst+ 8, linesize, bedge_lim);
        s->vp8dsp.vp8_h_loop_filter_simple(dst+12, linesize, bedge_lim);
    }

    if (mb_y)
        s->vp8dsp.vp8_v_loop_filter_simple(dst, linesize, mbedge_lim);
    if (inner_filter) {
        s->vp8dsp.vp8_v_loop_filter_simple(dst+ 4*linesize, linesize, bedge_lim);
        s->vp8dsp.vp8_v_loop_filter_simple(dst+ 8*linesize, linesize, bedge_lim);
        s->vp8dsp.vp8_v_loop_filter_simple(dst+12*linesize, linesize, bedge_lim);
    }
}

static void filter_mb_row(VP8Context *s, int mb_y)
{
    VP8FilterStrength *f = s->filter_strength;
    uint8_t *dst[3] = {
        s->framep[VP56_FRAME_CURRENT]->data[0] + 16*mb_y*s->linesize,
        s->framep[VP56_FRAME_CURRENT]->data[1] +  8*mb_y*s->uvlinesize,
        s->framep[VP56_FRAME_CURRENT]->data[2] +  8*mb_y*s->uvlinesize
    };
    int mb_x;

    for (mb_x = 0; mb_x < s->mb_width; mb_x++) {
        backup_mb_border(s->top_border[mb_x+1], dst[0], dst[1], dst[2], s->linesize, s->uvlinesize, 0);
        filter_mb(s, dst, f++, mb_x, mb_y);
        dst[0] += 16;
        dst[1] += 8;
        dst[2] += 8;
    }
}

static void filter_mb_row_simple(VP8Context *s, int mb_y)
{
    VP8FilterStrength *f = s->filter_strength;
    uint8_t *dst = s->framep[VP56_FRAME_CURRENT]->data[0] + 16*mb_y*s->linesize;
    int mb_x;

    for (mb_x = 0; mb_x < s->mb_width; mb_x++) {
        backup_mb_border(s->top_border[mb_x+1], dst, NULL, NULL, s->linesize, 0, 1);
        filter_mb_simple(s, dst, f++, mb_x, mb_y);
        dst += 16;
    }
}

static int vp8_decode_frame(AVCodecContext *avctx, void *data, int *data_size,
                            AVPacket *avpkt)
{
    VP8Context *s = avctx->priv_data;
    int ret, mb_x, mb_y, i, y, referenced;
    enum AVDiscard skip_thresh;
    AVFrame *av_uninit(curframe);

    if ((ret = decode_frame_header(s, avpkt->data, avpkt->size)) < 0)
        return ret;

    referenced = s->update_last || s->update_golden == VP56_FRAME_CURRENT
                                || s->update_altref == VP56_FRAME_CURRENT;

    skip_thresh = !referenced ? AVDISCARD_NONREF :
                    !s->keyframe ? AVDISCARD_NONKEY : AVDISCARD_ALL;

    if (avctx->skip_frame >= skip_thresh) {
        s->invisible = 1;
        goto skip_decode;
    }
    s->deblock_filter = s->filter.level && avctx->skip_loop_filter < skip_thresh;

    for (i = 0; i < 4; i++)
        if (&s->frames[i] != s->framep[VP56_FRAME_PREVIOUS] &&
            &s->frames[i] != s->framep[VP56_FRAME_GOLDEN] &&
            &s->frames[i] != s->framep[VP56_FRAME_GOLDEN2]) {
            curframe = s->framep[VP56_FRAME_CURRENT] = &s->frames[i];
            break;
        }
    if (curframe->data[0])
        avctx->release_buffer(avctx, curframe);

    curframe->key_frame = s->keyframe;
    curframe->pict_type = s->keyframe ? FF_I_TYPE : FF_P_TYPE;
    curframe->reference = referenced ? 3 : 0;
    if ((ret = avctx->get_buffer(avctx, curframe))) {
        av_log(avctx, AV_LOG_ERROR, "get_buffer() failed!\n");
        return ret;
    }

    // Given that arithmetic probabilities are updated every frame, it's quite likely
    // that the values we have on a random interframe are complete junk if we didn't
    // start decode on a keyframe. So just don't display anything rather than junk.
    if (!s->keyframe && (!s->framep[VP56_FRAME_PREVIOUS] ||
                         !s->framep[VP56_FRAME_GOLDEN] ||
                         !s->framep[VP56_FRAME_GOLDEN2])) {
        av_log(avctx, AV_LOG_WARNING, "Discarding interframe without a prior keyframe!\n");
        return AVERROR_INVALIDDATA;
    }

    s->linesize   = curframe->linesize[0];
    s->uvlinesize = curframe->linesize[1];

    if (!s->edge_emu_buffer)
        s->edge_emu_buffer = av_malloc(21*s->linesize);

    memset(s->top_nnz, 0, s->mb_width*sizeof(*s->top_nnz));

    /* Zero macroblock structures for top/top-left prediction from outside the frame. */
    memset(s->macroblocks + s->mb_height*2 - 1, 0, (s->mb_width+1)*sizeof(*s->macroblocks));

    // top edge of 127 for intra prediction
    memset(s->top_border, 127, (s->mb_width+1)*sizeof(*s->top_border));
    memset(s->ref_count, 0, sizeof(s->ref_count));
    if (s->keyframe)
        memset(s->intra4x4_pred_mode_top, DC_PRED, s->mb_width*4);

    for (mb_y = 0; mb_y < s->mb_height; mb_y++) {
        VP56RangeCoder *c = &s->coeff_partition[mb_y & (s->num_coeff_partitions-1)];
        VP8Macroblock *mb = s->macroblocks + (s->mb_height - mb_y - 1)*2;
        int mb_xy = mb_y*s->mb_width;
        uint8_t *dst[3] = {
            curframe->data[0] + 16*mb_y*s->linesize,
            curframe->data[1] +  8*mb_y*s->uvlinesize,
            curframe->data[2] +  8*mb_y*s->uvlinesize
        };

        memset(mb - 1, 0, sizeof(*mb));   // zero left macroblock
        memset(s->left_nnz, 0, sizeof(s->left_nnz));
        AV_WN32A(s->intra4x4_pred_mode_left, DC_PRED*0x01010101);

        // left edge of 129 for intra prediction
        if (!(avctx->flags & CODEC_FLAG_EMU_EDGE))
            for (i = 0; i < 3; i++)
                for (y = 0; y < 16>>!!i; y++)
                    dst[i][y*curframe->linesize[i]-1] = 129;
        if (mb_y)
            memset(s->top_border, 129, sizeof(*s->top_border));

        for (mb_x = 0; mb_x < s->mb_width; mb_x++, mb_xy++, mb++) {
            /* Prefetch the current frame, 4 MBs ahead */
            s->dsp.prefetch(dst[0] + (mb_x&3)*4*s->linesize + 64, s->linesize, 4);
            s->dsp.prefetch(dst[1] + (mb_x&7)*s->uvlinesize + 64, dst[2] - dst[1], 2);

            decode_mb_mode(s, mb, mb_x, mb_y, s->segmentation_map + mb_xy);

            prefetch_motion(s, mb, mb_x, mb_y, mb_xy, VP56_FRAME_PREVIOUS);

            if (!mb->skip)
                decode_mb_coeffs(s, c, mb, s->top_nnz[mb_x], s->left_nnz);

            if (mb->mode <= MODE_I4x4)
                intra_predict(s, dst, mb, mb_x, mb_y);
            else
                inter_predict(s, dst, mb, mb_x, mb_y);

            prefetch_motion(s, mb, mb_x, mb_y, mb_xy, VP56_FRAME_GOLDEN);

            if (!mb->skip) {
                idct_mb(s, dst, mb);
            } else {
                AV_ZERO64(s->left_nnz);
                AV_WN64(s->top_nnz[mb_x], 0);   // array of 9, so unaligned

                // Reset DC block predictors if they would exist if the mb had coefficients
                if (mb->mode != MODE_I4x4 && mb->mode != VP8_MVMODE_SPLIT) {
                    s->left_nnz[8]      = 0;
                    s->top_nnz[mb_x][8] = 0;
                }
            }

            if (s->deblock_filter)
                filter_level_for_mb(s, mb, &s->filter_strength[mb_x]);

            prefetch_motion(s, mb, mb_x, mb_y, mb_xy, VP56_FRAME_GOLDEN2);

            dst[0] += 16;
            dst[1] += 8;
            dst[2] += 8;
        }
        if (s->deblock_filter) {
            if (s->filter.simple)
                filter_mb_row_simple(s, mb_y);
            else
                filter_mb_row(s, mb_y);
        }
    }

skip_decode:
    // if future frames don't use the updated probabilities,
    // reset them to the values we saved
    if (!s->update_probabilities)
        s->prob[0] = s->prob[1];

    // check if golden and altref are swapped
    if (s->update_altref == VP56_FRAME_GOLDEN &&
        s->update_golden == VP56_FRAME_GOLDEN2)
        FFSWAP(AVFrame *, s->framep[VP56_FRAME_GOLDEN], s->framep[VP56_FRAME_GOLDEN2]);
    else {
        if (s->update_altref != VP56_FRAME_NONE)
            s->framep[VP56_FRAME_GOLDEN2] = s->framep[s->update_altref];

        if (s->update_golden != VP56_FRAME_NONE)
            s->framep[VP56_FRAME_GOLDEN] = s->framep[s->update_golden];
    }

    if (s->update_last) // move cur->prev
        s->framep[VP56_FRAME_PREVIOUS] = s->framep[VP56_FRAME_CURRENT];

    // release no longer referenced frames
    for (i = 0; i < 4; i++)
        if (s->frames[i].data[0] &&
            &s->frames[i] != s->framep[VP56_FRAME_CURRENT] &&
            &s->frames[i] != s->framep[VP56_FRAME_PREVIOUS] &&
            &s->frames[i] != s->framep[VP56_FRAME_GOLDEN] &&
            &s->frames[i] != s->framep[VP56_FRAME_GOLDEN2])
            avctx->release_buffer(avctx, &s->frames[i]);

    if (!s->invisible) {
        *(AVFrame*)data = *s->framep[VP56_FRAME_CURRENT];
        *data_size = sizeof(AVFrame);
    }

    return avpkt->size;
}

static av_cold int vp8_decode_init(AVCodecContext *avctx)
{
    VP8Context *s = avctx->priv_data;

    s->avctx = avctx;
    avctx->pix_fmt = PIX_FMT_YUV420P;

    dsputil_init(&s->dsp, avctx);
    ff_h264_pred_init(&s->hpc, CODEC_ID_VP8);
    ff_vp8dsp_init(&s->vp8dsp);

    // intra pred needs edge emulation among other things
    if (avctx->flags&CODEC_FLAG_EMU_EDGE) {
        av_log(avctx, AV_LOG_ERROR, "Edge emulation not supported\n");
        return AVERROR_PATCHWELCOME;
    }

    return 0;
}

static av_cold int vp8_decode_free(AVCodecContext *avctx)
{
    vp8_decode_flush(avctx);
    return 0;
}

AVCodec vp8_decoder = {
    "vp8",
    AVMEDIA_TYPE_VIDEO,
    CODEC_ID_VP8,
    sizeof(VP8Context),
    vp8_decode_init,
    NULL,
    vp8_decode_free,
    vp8_decode_frame,
    CODEC_CAP_DR1,
    .flush = vp8_decode_flush,
    .long_name = NULL_IF_CONFIG_SMALL("On2 VP8"),
};
