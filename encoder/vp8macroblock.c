/*****************************************************************************
 * macroblock.c: macroblock encoding
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

#include "common/common.h"
#include "common/vp8common.h"
#include "macroblock.h"

static void x264_vp8_mb_encode_i16x16( x264_t *h, int i_qp )
{
    pixel *p_src = h->mb.pic.p_fenc[0];
    pixel *p_dst = h->mb.pic.p_fdec[0];

    ALIGNED_ARRAY_16( dctcoef, dct4x4,[16],[16] );
    ALIGNED_ARRAY_16( dctcoef, dct_dc4x4,[16] );

    int nz, block_cbp = 0;
    int i_mode = h->mb.i_intra16x16_pred_mode;

    h->predict_16x16[i_mode]( h->mb.pic.p_fdec[0] );

    h->dctf.sub16x16_dct( dct4x4, p_src, p_dst );

    for( int i = 0; i < 16; i++ )
    {
        /* copy dc coeff */
        if( h->mb.b_noise_reduction )
            h->quantf.denoise_dct( dct4x4[i], h->nr_residual_sum[0], h->nr_offset[0], 16 );
        dct_dc4x4[i] = dct4x4[i][0];
        dct4x4[i][0] = 0;

        /* quant/dequant */
        nz = h->quantf.vp8quant_4x4( dct4x4[i], h->vp8quant_mf[1][i_qp], h->vp8quant_bias[1][i_qp] );
        h->mb.cache.non_zero_count[x264_raster8[i]] = nz;
        if( nz )
        {
            memcpy( h->dct.luma4x4[i], dct4x4[i], 16*sizeof(dctcoef) );
            h->quantf.vp8dequant_4x4( dct4x4[i], h->vp8dequant_mf[1][i_qp] );
            block_cbp = 0xf;
        }
    }

    h->mb.i_cbp_luma |= block_cbp;

    h->dctf.dct4x4dc( dct_dc4x4 );
    nz = h->quantf.vp8quant_4x4( dct_dc4x4, h->vp8quant_mf[0][i_qp], h->vp8quant_bias[0][i_qp] );

    h->mb.cache.non_zero_count[x264_scan8[LUMA_DC]] = nz;
    if( nz )
    {
        memcpy( h->dct.luma16x16_dc[0], dct_dc4x4, 16*sizeof(dctcoef) );

        h->quantf.vp8dequant_4x4( dct_dc4x4, h->vp8dequant_mf[0][i_qp] );

        h->dctf.idct4x4dc( dct_dc4x4 );

        for( int i = 0; i < 16; i++ )
            dct4x4[i][0] = dct_dc4x4[i];
    }

    /* put pixels to fdec */
    if( nz || block_cbp )
        h->dctf.add16x16_idct( p_dst, dct4x4 );
}

static void x264_vp8_mb_encode_chroma( x264_t *h, int i_qp )
{
    int nz;
    h->mb.i_cbp_chroma = 0;
    h->nr_count[2] += h->mb.b_noise_reduction * 4;

    for( int ch = 0; ch < 2; ch++ )
    {
        pixel *p_src = h->mb.pic.p_fenc[1+ch];
        pixel *p_dst = h->mb.pic.p_fdec[1+ch];
        int block_cbp = 0;

        ALIGNED_ARRAY_16( dctcoef, dct4x4,[4],[16] );

        h->dctf.sub8x8_dct( dct4x4, p_src, p_dst );

        if( h->mb.b_noise_reduction )
            for( int i = 0; i < 4; i++ )
                h->quantf.denoise_dct( dct4x4[i], h->nr_residual_sum[2], h->nr_offset[2], 16 );

        /* calculate dct coeffs */
        for( int i = 0; i < 4; i++ )
        {
            nz = h->quantf.vp8quant_4x4( dct4x4[i], h->vp8quant_mf[2][i_qp], h->vp8quant_bias[2][i_qp] );
            h->mb.cache.non_zero_count[x264_scan8[16+i+ch*16]] = nz;
            if( nz )
            {
                block_cbp = 1;
                memcpy( h->dct.luma4x4[16+i+ch*16], dct4x4[i], 16*sizeof(dctcoef) );
                h->quantf.vp8dequant_4x4( dct4x4[i], h->vp8dequant_mf[2][i_qp] );
            }
        }

        if( !block_cbp ) /* Whole block is empty */
            continue;

        h->mb.i_cbp_chroma = 1;
        h->dctf.add8x8_idct( p_dst, dct4x4 );
    }
}

void x264_vp8_macroblock_encode( x264_t *h )
{
    int i_qp = h->mb.i_qp;
    h->mb.i_cbp_luma = 0;
    h->mb.cache.non_zero_count[x264_scan8[LUMA_DC]] = 0;

    if( h->mb.i_type == I_16x16 )
    {
        x264_vp8_mb_encode_i16x16( h, i_qp );
    }
    else if( h->mb.i_type == I_4x4 )
    {
        /* TODO: I4x4 MB*/
    }
    else
    {
        /* TODO: Inter MB */
    }

    /* encode chroma */
    if( IS_INTRA( h->mb.i_type ) )
    {
        int i_mode = h->mb.i_chroma_pred_mode;

        h->predict_chroma[i_mode]( h->mb.pic.p_fdec[1] );
        h->predict_chroma[i_mode]( h->mb.pic.p_fdec[2] );
    }

    /* encode the 4x4 blocks */
    x264_vp8_mb_encode_chroma( h, h->mb.i_chroma_qp );

    /* store cbp */
    int cbp = h->mb.i_cbp_chroma << 4 | h->mb.i_cbp_luma;
    if( h->param.b_cabac )
        cbp |= h->mb.cache.non_zero_count[x264_scan8[LUMA_DC    ]] << 8
            |  h->mb.cache.non_zero_count[x264_scan8[CHROMA_DC+0]] << 9
            |  h->mb.cache.non_zero_count[x264_scan8[CHROMA_DC+1]] << 10;
    h->mb.cbp[h->mb.i_mb_xy] = cbp;
}
