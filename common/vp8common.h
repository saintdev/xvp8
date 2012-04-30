/*****************************************************************************
 * vp8common.h: vp8 specific common functions
 *****************************************************************************
  * Copyright (C) 2012 x264 project
  *
  * Author: Nathan Caldwell <saintdev@gmail.com>
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

#ifndef X264_VP8COMMON_H
#define X264_VP8COMMON_H

/* Luma only raster-order x264_scan8
 * See the table in the comment above x264_scan8 in common.h
 * x264_scan8 can be used for chroma in 4:2:0
 */
static const uint8_t x264_raster8[16] =
{
    4+ 1*8, 5+ 1*8, 6+ 1*8, 7+ 1*8,
    4+ 2*8, 5+ 2*8, 6+ 2*8, 7+ 2*8,
    4+ 3*8, 5+ 3*8, 6+ 3*8, 7+ 3*8,
    4+ 4*8, 5+ 4*8, 6+ 4*8, 7+ 4*8,
};

static const uint8_t vp8_block_idx_xy_fenc[16] =
{
    0*4 + 0*4*FENC_STRIDE, 1*4 + 0*4*FENC_STRIDE,
    2*4 + 0*4*FENC_STRIDE, 3*4 + 0*4*FENC_STRIDE,
    0*4 + 1*4*FENC_STRIDE, 1*4 + 1*4*FENC_STRIDE,
    2*4 + 1*4*FENC_STRIDE, 3*4 + 1*4*FENC_STRIDE,
    0*4 + 2*4*FENC_STRIDE, 1*4 + 2*4*FENC_STRIDE,
    2*4 + 2*4*FENC_STRIDE, 3*4 + 2*4*FENC_STRIDE,
    0*4 + 3*4*FENC_STRIDE, 1*4 + 3*4*FENC_STRIDE,
    2*4 + 3*4*FENC_STRIDE, 3*4 + 3*4*FENC_STRIDE
};
static const uint16_t vp8_block_idx_xy_fdec[16] =
{
    0*4 + 0*4*FDEC_STRIDE, 1*4 + 0*4*FDEC_STRIDE,
    2*4 + 0*4*FDEC_STRIDE, 3*4 + 0*4*FDEC_STRIDE,
    0*4 + 1*4*FDEC_STRIDE, 1*4 + 1*4*FDEC_STRIDE,
    2*4 + 1*4*FDEC_STRIDE, 3*4 + 1*4*FDEC_STRIDE,
    0*4 + 2*4*FDEC_STRIDE, 1*4 + 2*4*FDEC_STRIDE,
    2*4 + 2*4*FDEC_STRIDE, 3*4 + 2*4*FDEC_STRIDE,
    0*4 + 3*4*FDEC_STRIDE, 1*4 + 3*4*FDEC_STRIDE,
    2*4 + 3*4*FDEC_STRIDE, 3*4 + 3*4*FDEC_STRIDE
};

#endif

