/*****************************************************************************
 * vp8deblock.c: vp8 deblocking
 *****************************************************************************
 * Copyright (C) 2003-2012 x264 project
 *
 * Authors: Nathan Caldwell <saintdev@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at licensing@x264.com.
 *****************************************************************************/

#include "common.h"

static const uint8_t vp8_hev_threshold_table[2][64] =
{
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 1, 1, 1, 1, 1,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        3, 3, 3, 3
    },
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2,
    },
};

static const uint8_t vp8_interior_limit_table[64][8] =
{
    { 1, 1, 1, 1, 1, 1, 1, 1, }, { 1, 1, 1, 1, 1, 1, 1, 1, }, { 2, 1, 1, 1, 1, 1, 1, 1, },
    { 3, 1, 1, 1, 1, 1, 1, 1, }, { 4, 2, 2, 2, 2, 1, 1, 1, }, { 5, 2, 2, 2, 2, 1, 1, 1, },
    { 6, 3, 3, 3, 3, 1, 1, 1, }, { 7, 3, 3, 3, 3, 1, 1, 1, }, { 8, 4, 4, 4, 4, 2, 2, 2, },
    { 9, 4, 4, 4, 4, 2, 2, 2, }, {10, 5, 5, 5, 5, 2, 2, 2, }, {11, 5, 5, 5, 5, 2, 2, 2, },
    {12, 6, 6, 6, 5, 3, 3, 2, }, {13, 6, 6, 6, 5, 3, 3, 2, }, {14, 7, 7, 6, 5, 3, 3, 2, },
    {15, 7, 7, 6, 5, 3, 3, 2, }, {16, 8, 7, 6, 5, 4, 3, 2, }, {17, 8, 7, 6, 5, 4, 3, 2, },
    {18, 8, 7, 6, 5, 4, 3, 2, }, {19, 8, 7, 6, 5, 4, 3, 2, }, {20, 8, 7, 6, 5, 4, 3, 2, },
    {21, 8, 7, 6, 5, 4, 3, 2, }, {22, 8, 7, 6, 5, 4, 3, 2, }, {23, 8, 7, 6, 5, 4, 3, 2, },
    {24, 8, 7, 6, 5, 4, 3, 2, }, {25, 8, 7, 6, 5, 4, 3, 2, }, {26, 8, 7, 6, 5, 4, 3, 2, },
    {27, 8, 7, 6, 5, 4, 3, 2, }, {28, 8, 7, 6, 5, 4, 3, 2, }, {29, 8, 7, 6, 5, 4, 3, 2, },
    {30, 8, 7, 6, 5, 4, 3, 2, }, {31, 8, 7, 6, 5, 4, 3, 2, }, {32, 8, 7, 6, 5, 4, 3, 2, },
    {33, 8, 7, 6, 5, 4, 3, 2, }, {34, 8, 7, 6, 5, 4, 3, 2, }, {35, 8, 7, 6, 5, 4, 3, 2, },
    {36, 8, 7, 6, 5, 4, 3, 2, }, {37, 8, 7, 6, 5, 4, 3, 2, }, {38, 8, 7, 6, 5, 4, 3, 2, },
    {39, 8, 7, 6, 5, 4, 3, 2, }, {40, 8, 7, 6, 5, 4, 3, 2, }, {41, 8, 7, 6, 5, 4, 3, 2, },
    {42, 8, 7, 6, 5, 4, 3, 2, }, {43, 8, 7, 6, 5, 4, 3, 2, }, {44, 8, 7, 6, 5, 4, 3, 2, },
    {45, 8, 7, 6, 5, 4, 3, 2, }, {46, 8, 7, 6, 5, 4, 3, 2, }, {47, 8, 7, 6, 5, 4, 3, 2, },
    {48, 8, 7, 6, 5, 4, 3, 2, }, {49, 8, 7, 6, 5, 4, 3, 2, }, {50, 8, 7, 6, 5, 4, 3, 2, },
    {51, 8, 7, 6, 5, 4, 3, 2, }, {52, 8, 7, 6, 5, 4, 3, 2, }, {53, 8, 7, 6, 5, 4, 3, 2, },
    {54, 8, 7, 6, 5, 4, 3, 2, }, {55, 8, 7, 6, 5, 4, 3, 2, }, {56, 8, 7, 6, 5, 4, 3, 2, },
    {57, 8, 7, 6, 5, 4, 3, 2, }, {58, 8, 7, 6, 5, 4, 3, 2, }, {59, 8, 7, 6, 5, 4, 3, 2, },
    {60, 8, 7, 6, 5, 4, 3, 2, }, {61, 8, 7, 6, 5, 4, 3, 2, }, {62, 8, 7, 6, 5, 4, 3, 2, },
    {63, 8, 7, 6, 5, 4, 3, 2, },
};

/* QP*3/8 */
const uint8_t x264_vp8_deblock_level_table[VP8_QP_MAX+1] =
{
     0,  0,  0,  1,  1,  1,  2,  2,  3,  3,
     3,  4,  4,  4,  5,  5,  6,  6,  6,  7,
     7,  7,  8,  8,  9,  9,  9, 10, 10, 10,
    11, 11, 12, 12, 12, 13, 13, 13, 14, 14,
    15, 15, 15, 16, 16, 16, 17, 17, 18, 18,
    18, 19, 19, 19, 20, 20, 21, 21, 21, 22,
    22, 22, 23, 23, 24, 24, 24, 25, 25, 25,
    26, 26, 27, 27, 27, 28, 28, 28, 29, 29,
    30, 30, 30, 31, 31, 31, 32, 32, 33, 33,
    33, 34, 34, 34, 35, 35, 36, 36, 36, 37,
    37, 37, 38, 38, 39, 39, 39, 40, 40, 40,
    41, 41, 42, 42, 42, 43, 43, 43, 44, 44,
    45, 45, 45, 46, 46, 46, 47, 47,
};

static ALWAYS_INLINE void x264_vp8_macroblock_deblock( x264_t *h, int level, int sharpness, int mb_x, int mb_y )
{
    int stridey  = h->fdec->i_stride[0];
    int strideuv = h->fdec->i_stride[1];
    int interior = vp8_interior_limit_table[level][sharpness];
    int edge = level * 2 + interior;
    int hev = vp8_hev_threshold_table[h->sh.i_type == SLICE_TYPE_I][level];

    pixel *pixy = h->fdec->plane[0] + 16*mb_y*stridey  + 16*mb_x;
    pixel *pixuv = h->fdec->plane[1] + 8*mb_y*strideuv + 16*mb_x;

    if( !level )
        return;

    if( h->mb.i_neighbour & MB_LEFT )
    {
        h->loopf.vp8_deblock_luma_edge[0]( pixy, stridey, edge + 4, interior, hev );
        h->loopf.vp8_deblock_chroma_edge[0]( pixuv, strideuv, edge + 4, interior, hev );
    }

    if( h->mb.cbp[h->mb.i_mb_xy] || h->mb.type[h->mb.i_mb_xy] == I_4x4 )
    {
        for( int i = 4; i < 16; i += 4 )
            h->loopf.vp8_deblock_luma[0]( pixy + i, stridey, edge, interior, hev );
        h->loopf.vp8_deblock_chroma[0]( pixuv + 8, strideuv, edge, interior, hev );
    }

    if( h->mb.i_neighbour & MB_TOP )
    {
        h->loopf.vp8_deblock_luma_edge[1]( pixy, stridey, edge + 4, interior, hev );
        h->loopf.vp8_deblock_chroma_edge[1]( pixuv, strideuv, edge + 4, interior, hev );
    }

    if( h->mb.cbp[h->mb.i_mb_xy] || h->mb.type[h->mb.i_mb_xy] == I_4x4 )
    {
        for( int i = 4; i < 16; i += 4 )
            h->loopf.vp8_deblock_luma[1]( pixy + i * stridey, stridey, edge, interior, hev );
        h->loopf.vp8_deblock_chroma[1]( pixuv + 4 * strideuv, strideuv, edge, interior, hev );
    }
}

void x264_vp8_deblock_row( x264_t *h, int mb_y )
{
    /* Formula for I frames taken from libvpx. Something similar for P frames?
     * Match h264 deblocking curve? */
    int level = h->sh.i_type == SLICE_TYPE_I ? x264_vp8_deblock_level_table[h->sh.i_qp] : 0;
    /* What to do for sharpness? libvpx always sets sharpness to 0 for keyframes. */
    int sharpness = 0;

    for( int mb_x = 0; mb_x < h->mb.i_mb_width; mb_x++ )
    {
        x264_prefetch_fenc( h, h->fdec, mb_x, mb_y );
        h->mb.i_mb_xy = mb_y * h->mb.i_mb_stride + mb_x;
        h->mb.i_neighbour = 0;
        if( mb_x )
            h->mb.i_neighbour |= MB_LEFT;
        if( mb_y )
            h->mb.i_neighbour |= MB_TOP;

        x264_vp8_macroblock_deblock( h, level, sharpness, mb_x, mb_y );
    }
}
