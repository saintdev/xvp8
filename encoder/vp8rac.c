/*****************************************************************************
 * vp8rac.c: vp8 rac bitstream writing
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

static void x264_vp8rac_intra4x4_pred_mode( x264_t *h, x264_vp8rac_t *rc, int i_mode, const uint8_t prob[9] )
{
    if( i_mode == I_PRED_4x4_DC )
        x264_vp8rac_encode_decision( rc, prob[0], 0 );  /* B_DC_PRED */
    else
    {
        x264_vp8rac_encode_decision( rc, prob[0], 1 );
        if( i_mode == I_PRED_4x4_TM )
            x264_vp8rac_encode_decision( rc, prob[1], 0 );  /* B_TM_PRED */
        else
        {
            x264_vp8rac_encode_decision( rc, prob[1], 1 );
            if( i_mode == I_PRED_4x4_V )
                x264_vp8rac_encode_decision( rc, prob[2], 0 );  /* B_VE_PRED */
            else
            {
                x264_vp8rac_encode_decision( rc, prob[2], 1 );
                if( i_mode == I_PRED_4x4_DDL || i_mode >= I_PRED_4x4_HD )
                {
                    x264_vp8rac_encode_decision( rc, prob[3], 1 );
                    if( i_mode == I_PRED_4x4_DDL )
                        x264_vp8rac_encode_decision( rc, prob[6], 0 );  /* B_LD_PRED */
                    else
                    {
                        int a = !(i_mode & 1);
                        x264_vp8rac_encode_decision( rc, prob[6], 1 );
                        x264_vp8rac_encode_decision( rc, prob[7], a );  /* B_VL_PRED */
                        if( a )
                            x264_vp8rac_encode_decision( rc, prob[8], i_mode >> 3 );    /* B_HD_PRED/B_HU_PRED */
                    }
                }
                else
                {
                    int a = i_mode >> 2;
                    x264_vp8rac_encode_decision( rc, prob[3], 0 );

                    x264_vp8rac_encode_decision( rc, prob[4], a );  /* B_HE_PRED */
                    if( a )
                        x264_vp8rac_encode_decision( rc, prob[5], i_mode&1 );   /* B_RD_PRED/B_VR_PRED */
                }
            }
        }
    }
}

static void x264_vp8rac_intra_chroma_pred_mode( x264_t *h, x264_vp8rac_t *rc, int i_mode, const uint8_t prob[3] )
{
    if( i_mode != I_PRED_CHROMA_DC )
    {
        x264_vp8rac_encode_decision( rc, prob[0], 1 );
        if( i_mode != I_PRED_CHROMA_V )
        {
            x264_vp8rac_encode_decision( rc, prob[1], 1 );
            x264_vp8rac_encode_decision( rc, prob[2], i_mode>>1 );
        }
        else
            x264_vp8rac_encode_decision( rc, prob[1], 0 );
    }
    else
        x264_vp8rac_encode_decision( rc, prob[0], 0 );
}

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

static ALWAYS_INLINE void x264_vp8rac_mb_header_i( x264_t *h, x264_vp8rac_t *rc, int i_mb_type )
{
    /* mb type */
    if( i_mb_type == I_4x4 )
    {
        x264_vp8rac_encode_decision( rc, 145, 0 );
        for( int i = 0; i < 16; i++ )
        {
            int i_mode = x264_mb_pred_mode4x4_fix( h->mb.cache.intra4x4_pred_mode[x264_raster8[i]] );
            int left = x264_mb_pred_mode4x4_fix( h->mb.cache.intra4x4_pred_mode[x264_raster8[i] - 1] );
            int top  = x264_mb_pred_mode4x4_fix( h->mb.cache.intra4x4_pred_mode[x264_raster8[i] - 8] );
            top = top < 0 ? I_PRED_4x4_DC : top;
            left = left < 0 ? I_PRED_4x4_DC : left;
            x264_vp8rac_intra4x4_pred_mode( h, rc, i_mode, x264_vp8_intra_i4x4_pred_probs[top][left] );
        }
    }
    else
    {
        int pred = x264_mb_pred_mode16x16_fix[h->mb.i_intra16x16_pred_mode];
        int a = pred & 1;
        int b = pred >> 1;

        /* i16x16 pred modes */
        x264_vp8rac_encode_decision( rc, 145, 1 );
        x264_vp8rac_encode_decision( rc, 156, a );
        x264_vp8rac_encode_decision( rc, a?128:163, a?b:!b );
    }

    /* chroma pred mode */
    int pred = x264_mb_chroma_pred_mode_fix[h->mb.i_chroma_pred_mode];
    x264_vp8rac_intra_chroma_pred_mode( h, rc, pred, x264_vp8_intra_chroma_pred_probs[0] );
}

static ALWAYS_INLINE void x264_vp8rac_mb_header_p( x264_t *h, x264_vp8rac_t *rc, int i_mb_type )
{
    x264_vp8rac_encode_bypass( rc, !IS_INTRA( i_mb_type ) );

    if( i_mb_type == P_L0 )
    {
        /* Previous frame only, no golden/altref for now. */
        x264_vp8rac_encode_bypass( rc, 0 );

    }
    else if( i_mb_type == P_8x8)
    {

    }
    else /* intra */
    {
        if( i_mb_type == I_4x4 )
        {
            x264_vp8rac_encode_decision( rc, 112, 1 );
            x264_vp8rac_encode_decision( rc, 86, 1 );
            x264_vp8rac_encode_decision( rc, 37, 1 );
            for( int i = 0; i < 16; i++ )
            {
                int i_mode = x264_mb_pred_mode4x4_fix( h->mb.cache.intra4x4_pred_mode[x264_raster8[i]] );
                x264_vp8rac_intra4x4_pred_mode( h, rc, i_mode, x264_vp8_inter_i4x4_pred_probs );
            }
        }
        else
        {
            int pred = x264_mb_pred_mode16x16_fix[h->mb.i_intra16x16_pred_mode];
            int a = pred & 1;
            int b = pred >> 1;

            /* i16x16 pred modes */
            x264_vp8rac_encode_decision( rc, 112, b?a:1 );
            if( (b && a) || !b )
            {
                x264_vp8rac_encode_decision( rc, 86, b );
                x264_vp8rac_encode_decision( rc, b?37:140, b?0:a );
            }
        }
        /* intra chroma pred mode */
        int pred = x264_mb_chroma_pred_mode_fix[h->mb.i_chroma_pred_mode];
        x264_vp8rac_intra_chroma_pred_mode( h, rc, pred, x264_vp8_intra_chroma_pred_probs[1] );
    }
}

void x264_macroblock_write_vp8rac( x264_t *h, x264_vp8rac_t *partition_rac )
{
    x264_vp8rac_t *cb = &h->vp8.header_rac;
    const int i_mb_type = h->mb.i_type;
#if !RDO_SKIP_BS
    const int i_mb_pos_start = x264_vp8rac_pos( cb );
    int       i_mb_pos_tex;
#endif

    x264_vp8rac_encode_decision( cb, 0x80, !h->mb.cbp[h->mb.i_mb_xy] );

    if( h->sh.i_type == SLICE_TYPE_P )
        x264_vp8rac_mb_header_p( h, cb, i_mb_type );
    else   /* h->sh.i_type == SLICE_TYPE_I */
        x264_vp8rac_mb_header_i( h, cb, i_mb_type );
#if !RDO_SKIP_BS
    i_mb_pos_tex = x264_vp8rac_pos( cb );
    h->stat.frame.i_mv_bits += i_mb_pos_tex - i_mb_pos_start;
#endif

    if( h->mb.cbp[h->mb.i_mb_xy] )
    {
        /* write residual */
        if( i_mb_type == I_16x16 )
        {
            int t = h->mb.last_luma_dc_top[h->mb.i_mb_x];
            int l = h->mb.last_luma_dc_left;

            /* DC Luma */
            x264_vp8rac_block_residual_dc_cbf( h, partition_rac, x264_vp8_default_dct_probs[1], x264_scan8[LUMA_DC], h->dct.luma16x16_dc[0], t + l );

            /* AC Luma */
            for( int i = 0; i < 16; i++ )
            {
                t = h->mb.cache.non_zero_count[x264_raster8[i] - 8] & 0x7f;
                l = h->mb.cache.non_zero_count[x264_raster8[i] - 1] & 0x7f;
                x264_vp8rac_block_residual_cbf( h, partition_rac, x264_vp8_default_dct_probs[0], x264_raster8[i], h->dct.luma4x4[i], t + l );
            }
        }
        else
        {
            for( int i = 0; i < 16; i++ )
            {
                int t = h->mb.cache.non_zero_count[x264_raster8[i] - 8] & 0x7f;
                int l = h->mb.cache.non_zero_count[x264_raster8[i] - 1] & 0x7f;
                x264_vp8rac_block_residual_dc_cbf( h, partition_rac, x264_vp8_default_dct_probs[3], x264_raster8[i], h->dct.luma4x4[i], t + l );
            }
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
