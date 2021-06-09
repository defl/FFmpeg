/*
 * r12b decoder
 *
 * Copyright (c) 2021 Dennis Fleurbaaij <mail@dennisfleurbaaij.com>
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

#include "avcodec.h"
#include "internal.h"

#define WORDS_PER_BLOCK 9
#define PIXELS_PER_BLOCK 8
#define BYTES_PER_BLOCK 36

static av_cold int decode_init(AVCodecContext *avctx)
{
    avctx->pix_fmt = AV_PIX_FMT_GBRP12LE;
    avctx->bits_per_raw_sample = 12;

    return 0;
}

static int decode_frame(AVCodecContext *avctx, void *data, int *got_frame,
                        AVPacket *avpkt)
{
    int ret;
    uint8_t *g_line, *b_line, *r_line;

    AVFrame *pic = data;
    const uint8_t* src = avpkt->data;
    const int blocks_per_line = avctx->width / PIXELS_PER_BLOCK;

    if (avctx->width % PIXELS_PER_BLOCK != 0) {
        av_log(avctx, AV_LOG_ERROR, "image width not modulo 8\n");
        return AVERROR_INVALIDDATA;
    }

    if (avpkt->size < avctx->height * blocks_per_line * BYTES_PER_BLOCK) {
        av_log(avctx, AV_LOG_ERROR, "packet too small\n");
        return AVERROR_INVALIDDATA;
    }

    pic->pict_type = AV_PICTURE_TYPE_I;
    pic->key_frame = 1;

    if ((ret = ff_get_buffer(avctx, pic, 0)) < 0)
        return ret;

    g_line = pic->data[0];
    b_line = pic->data[1];
    r_line = pic->data[2];

    for (int h = 0; h < avctx->height; h++) {
        uint16_t *g_dst = (uint16_t *)g_line;
        uint16_t *b_dst = (uint16_t *)b_line;
        uint16_t *r_dst = (uint16_t *)r_line;

        for (int w = 0; w < blocks_per_line; w++) {

            // This is an encoding from the table on page 213 of the BlackMagic
            // Decklink SDK pdf, version 12.0. Few helper defines to directly link
            // the naming in the doc to the code.

            #define GET_FF(word, byte) (*(src + ((word * 4) + byte)))
            #define GET_0F(word, byte) (GET_FF(word, byte) & 0x0F)
            #define GET_F0(word, byte) (GET_FF(word, byte) >> 4)
            #define PUT(dst, pixel) (*(dst + pixel))

            PUT(b_dst, 0) = GET_FF(0, 0) | GET_0F(1, 3) << 8;
            PUT(g_dst, 0) = GET_F0(0, 2) | GET_FF(0, 1) << 4;
            PUT(r_dst, 0) = GET_FF(0, 3) | GET_0F(0, 2) << 8;

            PUT(b_dst, 1) = GET_F0(1, 0) | GET_FF(2, 3) << 4;
            PUT(g_dst, 1) = GET_FF(1, 1) | GET_0F(1, 0) << 8;
            PUT(r_dst, 1) = GET_F0(1, 3) | GET_FF(1, 2) << 4;

            PUT(b_dst, 2) = GET_FF(3, 3) | GET_0F(3, 2) << 8;
            PUT(g_dst, 2) = GET_F0(2, 1) | GET_FF(2, 0) << 4;
            PUT(r_dst, 2) = GET_FF(2, 2) | GET_0F(2, 1) << 8;

            PUT(b_dst, 3) = GET_F0(4, 3) | GET_FF(4, 2) << 4;
            PUT(g_dst, 3) = GET_FF(3, 0) | GET_0F(4, 3) << 8;
            PUT(r_dst, 3) = GET_F0(3, 2) | GET_FF(3, 1) << 4;

            PUT(b_dst, 4) = GET_FF(5, 2) | GET_0F(5, 1) << 8;
            PUT(g_dst, 4) = GET_F0(4, 0) | GET_FF(5, 3) << 4;
            PUT(r_dst, 4) = GET_FF(4, 1) | GET_0F(4, 0) << 8;

            PUT(b_dst, 5) = GET_F0(6, 2) | GET_FF(6, 1) << 4;
            PUT(g_dst, 5) = GET_FF(6, 3) | GET_0F(6, 2) << 8;
            PUT(r_dst, 5) = GET_F0(5, 1) | GET_FF(5, 0) << 4;

            PUT(b_dst, 6) = GET_FF(7, 1) | GET_0F(7, 0) << 8;
            PUT(g_dst, 6) = GET_F0(7, 3) | GET_FF(7, 2) << 4;
            PUT(r_dst, 6) = GET_FF(6, 0) | GET_0F(7, 3) << 8;

            PUT(b_dst, 7) = GET_F0(8, 1) | GET_FF(8, 0) << 4;
            PUT(g_dst, 7) = GET_FF(8, 2) | GET_0F(8, 1) << 8;
            PUT(r_dst, 7) = GET_F0(7, 0) | GET_FF(8, 3) << 4;

            src += BYTES_PER_BLOCK;
            b_dst += PIXELS_PER_BLOCK;
            g_dst += PIXELS_PER_BLOCK;
            r_dst += PIXELS_PER_BLOCK;
        }

        g_line += pic->linesize[0];
        b_line += pic->linesize[1];
        r_line += pic->linesize[2];
    }

    *got_frame = 1;

    return avpkt->size;
}

const AVCodec ff_r12b_decoder = {
    .name           = "r12b",
    .long_name      = NULL_IF_CONFIG_SMALL("Uncompressed RGB 12-bit 8px in 36B"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_R12B,
    .init           = decode_init,
    .decode         = decode_frame,
    .capabilities   = AV_CODEC_CAP_DR1,
    .caps_internal  = FF_CODEC_CAP_INIT_THREADSAFE,
};
