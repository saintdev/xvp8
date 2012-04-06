/*****************************************************************************
 * cabac.c: cabac bitstream writing
 *****************************************************************************
 * Copyright (C) 2003-2012 x264 project
 *
 * Authors: Nathan Caldwell <saintdev@gmail.com>
 *          Jason Garrett-Glaser <darkshikari@gmail.com>
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

#include "common/common.h"
#include "common/vp8common.h"
#include "macroblock.h"

#ifndef RDO_SKIP_BS
#define RDO_SKIP_BS 0
#endif

static const uint8_t dct_cat1_prob[] = { 159, 0 };
static const uint8_t dct_cat2_prob[] = { 165, 145, 0 };
static const uint8_t dct_cat3_prob[] = { 173, 148, 140, 0 };
static const uint8_t dct_cat4_prob[] = { 176, 155, 140, 135, 0 };
static const uint8_t dct_cat5_prob[] = { 180, 157, 141, 134, 130, 0 };
static const uint8_t dct_cat6_prob[] = { 254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129, 0 };

static const uint8_t *const dct_cat_probs[] =
{
    dct_cat1_prob, dct_cat2_prob,
    dct_cat3_prob, dct_cat4_prob,
    dct_cat5_prob, dct_cat6_prob,
};

static const uint8_t dct_cat_bits[] = { 1, 2, 3, 4, 5, 11 };

/* Write a single DCT coefficient */
static ALWAYS_INLINE void x264_vp8rac_block_residual_coeff( x264_t *h, x264_vp8rac_t *rc, const uint8_t prob[NUM_DCT_TOKENS-1], dctcoef abs_coeff, int sign )
{
    x264_vp8rac_encode_decision( rc, prob[1], 1 );  // !DCT_0
    if( abs_coeff > 1 )
    {
        x264_vp8rac_encode_decision( rc, prob[2], 1 );
        if( abs_coeff < 5 )
        {
            x264_vp8rac_encode_decision( rc, prob[3], 0 );
            if( abs_coeff > 2 )
            {
                x264_vp8rac_encode_decision( rc, prob[4], 1 );
                x264_vp8rac_encode_decision( rc, prob[5], abs_coeff > 3 );    // DCT_3/DCT_4
            } else
                x264_vp8rac_encode_decision( rc, prob[4], 0 );  // DCT_2
        }
        else
        {
            x264_vp8rac_encode_decision( rc, prob[3], 1 );
            if( abs_coeff < 11 )
            {
                int a = abs_coeff > 6;

                abs_coeff -= 5 + (a << 1);

                x264_vp8rac_encode_decision( rc, prob[6], 0 );
                x264_vp8rac_encode_decision( rc, prob[7], a );  // CAT_1/CAT_2
                x264_vp8rac_encode_decision( rc, dct_cat_probs[a][0], (abs_coeff >> a) & 1 );
                if( a )
                    x264_vp8rac_encode_decision( rc, dct_cat2_prob[1], abs_coeff & 1 );
            }
            else
            {
                int a = abs_coeff > 34;
                int b = a ? abs_coeff > 66 : abs_coeff > 18;
                int cat = 2 + (a << 1) + b;
                const uint8_t *cat_prob = dct_cat_probs[cat];
                int i = 0;

                abs_coeff -= 3 + (2 << cat);

                x264_vp8rac_encode_decision( rc, prob[6], 1 );
                x264_vp8rac_encode_decision( rc, prob[8], a );
                x264_vp8rac_encode_decision( rc, prob[9+a], b );    // CAT_3-6

                for( int bits = dct_cat_bits[cat] - 1; bits >= 0; bits-- )
                    x264_vp8rac_encode_decision( rc, cat_prob[i++], (abs_coeff >> bits) & 1 );
            }
        }
    } else
        x264_vp8rac_encode_decision( rc, prob[2], 0 );  // DCT_1
    x264_vp8rac_encode_bypass( rc, sign );
}

static ALWAYS_INLINE void x264_vp8rac_block_residual( x264_t *h, x264_vp8rac_t *rc, const uint8_t probs[8][3][NUM_DCT_TOKENS-1], dctcoef *l, int nnz_pred, int ac_luma )
{
    int zeros = 0;

    for( int i = ac_luma; i < 16; i++ )
    {
        if( !l[i] )
        {
            zeros++;
            continue;
        }

        x264_vp8rac_encode_decision( rc, probs[x264_vp8_coeff_band[i - zeros]][nnz_pred][0], 1 ); // !EOB
        for( int j = i - zeros; j < i; j++ )
        {
            x264_vp8rac_encode_decision( rc, probs[x264_vp8_coeff_band[j]][nnz_pred][1], 0 ); // DCT_0
            nnz_pred = 0;
        }

        int abs_coeff = abs( l[i] );
        x264_vp8rac_block_residual_coeff( h, rc, probs[x264_vp8_coeff_band[i]][nnz_pred], abs_coeff, l[i] >> 31 );
        nnz_pred = 1 + (abs_coeff > 1);

        zeros = 0;
    }

    if( zeros )
        x264_vp8rac_encode_decision( rc, probs[x264_vp8_coeff_band[16-zeros]][nnz_pred][0], 0 );  // EOB
}

static ALWAYS_INLINE void x264_vp8rac_block_residual_cbf_internal( x264_t *h, x264_vp8rac_t *rc, const uint8_t probs[8][3][NUM_DCT_TOKENS-1], int idx, dctcoef *l, int nnz_pred, int ac_luma )
{
    if( h->mb.cache.non_zero_count[idx] )
        x264_vp8rac_block_residual( h, rc, probs, l, nnz_pred, ac_luma );
    else
        x264_vp8rac_encode_decision( rc, probs[x264_vp8_coeff_band[ac_luma]][nnz_pred][0], 0 );
}

#define x264_vp8rac_block_residual_dc_cbf( h, rc, probs, idx, l, nnz_pred )\
    x264_vp8rac_block_residual_cbf_internal( h, rc, probs, idx, l, nnz_pred, 0 )

#define x264_vp8rac_block_residual_cbf( h, rc, probs, idx, l, nnz_pred )\
    x264_vp8rac_block_residual_cbf_internal( h, rc, probs, idx, l, nnz_pred, 1 )

void x264_macroblock_write_vp8rac( x264_t *h, x264_vp8rac_t *partition_rac )
{
    x264_vp8rac_t *cb = &h->vp8.header_rac;
    const int i_mb_type = h->mb.i_type;
#if !RDO_SKIP_BS
    const int i_mb_pos_start = x264_vp8rac_pos( cb );
    int       i_mb_pos_tex;
#endif

    /* Force noskip for now */
    x264_vp8rac_encode_decision( cb, 0x80, 0 );

    if( h->sh.i_type == SLICE_TYPE_I )
    {
        /* mb type */
        if( h->mb.i_type == I_4x4 )
        {
            x264_vp8rac_encode_decision( cb, 145, 0 );
            /* TODO: put i4x4 modes here */
        }
        else
        {
            int pred = x264_mb_pred_mode16x16_fix[h->mb.i_intra16x16_pred_mode];
            /* i16x16 pred modes */
            x264_vp8rac_encode_decision( cb, 145, 1 );
            x264_vp8rac_encode_decision( cb, 156, pred&1 );
            x264_vp8rac_encode_decision( cb, (pred&1)?128:163, pred>>1 );
        }

        /* chroma pred mode */
        int pred = x264_mb_chroma_pred_mode_fix[h->mb.i_chroma_pred_mode];
        if( pred != I_PRED_CHROMA_DC )
        {
            x264_vp8rac_encode_decision( cb, 142, 1 );
            if( pred != I_PRED_CHROMA_V )
            {
                x264_vp8rac_encode_decision( cb, 114, 1 );
                x264_vp8rac_encode_decision( cb, 183, pred>>1 );
            }
            else
                x264_vp8rac_encode_decision( cb, 114, 0 );
        }
        else
            x264_vp8rac_encode_decision( cb, 142, 0 );
    }
#if !RDO_SKIP_BS
    i_mb_pos_tex = x264_vp8rac_pos( cb );
    h->stat.frame.i_mv_bits += i_mb_pos_tex - i_mb_pos_start;
#endif

    if( h->mb.i_cbp_luma || h->mb.i_cbp_chroma || i_mb_type == I_16x16 )
    {
        /* write residual */
        if( i_mb_type == I_16x16 )
        {
            int nnz_pred = 0;

            nnz_pred += h->mb.cache.i_cbp_top == -1 ? 0 : !!(h->mb.cache.i_cbp_top & 0x100);
            nnz_pred += h->mb.cache.i_cbp_left == -1 ? 0 : !!(h->mb.cache.i_cbp_left & 0x100);

            /* DC Luma */
            x264_vp8rac_block_residual_dc_cbf( h, partition_rac, x264_vp8_default_dct_probs[1], x264_scan8[LUMA_DC], h->dct.luma16x16_dc[0], nnz_pred );

            /* AC Luma */
            for( int i = 0; i < 16; i++ )
            {
                int t = h->mb.cache.non_zero_count[x264_raster8[i] - 8] & 0x7f;
                int l = h->mb.cache.non_zero_count[x264_raster8[i] - 1] & 0x7f;
                x264_vp8rac_block_residual_cbf( h, partition_rac, x264_vp8_default_dct_probs[0], x264_raster8[i], h->dct.luma4x4[i], t + l );
            }
        }
        else
        {
            /* TODO I_4x4 */
        }

        /* Chroma residual */
        for( int i = 16; i < 3*16; i += 16 )
            for( int j = i; j < i+4; j++ )
            {
                int t = h->mb.cache.non_zero_count[x264_scan8[j] - 8] & 0x7f;
                int l = h->mb.cache.non_zero_count[x264_scan8[j] - 1] & 0x7f;
                x264_vp8rac_block_residual_dc_cbf( h, partition_rac, x264_vp8_default_dct_probs[2], x264_scan8[j], h->dct.luma4x4[j], t + l );
            }
    }

#if !RDO_SKIP_BS
    h->stat.frame.i_tex_bits += x264_vp8rac_pos( cb ) - i_mb_pos_tex;
#endif
}
