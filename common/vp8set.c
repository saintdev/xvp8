/*****************************************************************************
 * vp8set.c: vp8 quantization init
 *****************************************************************************
 * Copyright (C) 2012 x264 project
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

static const uint8_t x264_vp8_dc_dequant[VP8_QP_MAX+1] =
{
    4,   5,   6,   7,   8,   9,  10,  10,  11,  12,  13,  14,  15,  16,  17,  17,
    18,  19,  20,  20,  21,  21,  22,  22,  23,  23,  24,  25,  25,  26,  27,  28,
    29,  30,  31,  32,  33,  34,  35,  36,  37,  37,  38,  39,  40,  41,  42,  43,
    44,  45,  46,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,
    59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,
    75,  76,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,
    91,  93,  95,  96,  98, 100, 101, 102, 104, 106, 108, 110, 112, 114, 116, 118,
    122, 124, 126, 128, 130, 132, 134, 136, 138, 140, 143, 145, 148, 151, 154, 157,
};

static const uint16_t x264_vp8_ac_dequant[VP8_QP_MAX+1] =
{
    4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,
    20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,
    36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,
    52,  53,  54,  55,  56,  57,  58,  60,  62,  64,  66,  68,  70,  72,  74,  76,
    78,  80,  82,  84,  86,  88,  90,  92,  94,  96,  98, 100, 102, 104, 106, 108,
    110, 112, 114, 116, 119, 122, 125, 128, 131, 134, 137, 140, 143, 146, 149, 152,
    155, 158, 161, 164, 167, 170, 173, 177, 181, 185, 189, 193, 197, 201, 205, 209,
    213, 217, 221, 225, 229, 234, 239, 245, 249, 254, 259, 264, 269, 274, 279, 284,
};

#define DIV(n,d) (((n) + ((d)>>1)) / (d))

int x264_vp8_cqm_init( x264_t *h )
{
    for( int i = 0; i < 3; i++ )
    {
        CHECKED_MALLOC( h->vp8quant_mf[i], (VP8_QP_MAX+1)*2*sizeof(udctcoef) );
        CHECKED_MALLOC( h->vp8dequant_mf[i], (VP8_QP_MAX+1)*2*sizeof(int) );
        CHECKED_MALLOC( h->vp8quant_bias[i], (VP8_QP_MAX+1)*2*sizeof(udctcoef) );
    }

    for( int i = 0; i < VP8_QP_MAX + 1; i++ )
    {
        int dc = x264_vp8_dc_dequant[i];
        int ac = x264_vp8_ac_dequant[i];

        /* FIXME: Quantized coefficients are off by one if the coefficient is
         * an even multiple of the quantizer.
         * FIXME: is this still true?
         */
        for( int j = 0; j < 3; j++ )
        {
            h->vp8dequant_mf[j][i][0] = j ? dc : 2 * dc;
            h->vp8dequant_mf[j][i][1] = j ? ac : 155 * ac / 100;
        }
        h->vp8dequant_mf[0][i][1] = X264_MAX( h->vp8dequant_mf[0][i][1], 8 );
        h->vp8dequant_mf[2][i][0] = X264_MIN( h->vp8dequant_mf[2][i][0], 132 );

        for( int j = 0; j < 3; j++ )
        {
            h->vp8quant_mf[j][i][0] = dc = DIV( 1<<16, h->vp8dequant_mf[j][i][0] );
            h->vp8quant_mf[j][i][1] = ac = DIV( 1<<16, h->vp8dequant_mf[j][i][1] );
            h->vp8quant_bias[j][i][0] = DIV( 1<<15, dc );
            h->vp8quant_bias[j][i][1] = DIV( 1<<15, ac );
        }
    }

    return 0;
fail:
    x264_vp8_cqm_delete( h );
    return -1;
}

void x264_vp8_cqm_delete( x264_t *h )
{
    for( int i = 0; i < 3; i++ )
    {
        x264_free( h->vp8quant_mf[i] );
        x264_free( h->vp8dequant_mf[i] );
        x264_free( h->vp8quant_bias[i] );
    }
}
