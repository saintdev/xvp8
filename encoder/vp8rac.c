/*****************************************************************************
 * cabac.c: cabac bitstream writing
 *****************************************************************************
 * Copyright (C) 2003-2012 x264 project
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Loren Merritt <lorenm@u.washington.edu>
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
#include "macroblock.h"

#ifndef RDO_SKIP_BS
#define RDO_SKIP_BS 0
#endif

void x264_macroblock_write_vp8rac( x264_t *h, x264_vp8rac_t *partition_rac )
{
    x264_vp8rac_t *cb = &h->vp8.header_rac;
#if !RDO_SKIP_BS
    const int i_mb_pos_start = x264_vp8rac_pos( cb );
    int       i_mb_pos_tex;
#endif

    /* Force skip for now */
    x264_vp8rac_encode_decision( cb, 0x80, 1 );

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

    /* We'll do the residual stuff later... */

#if !RDO_SKIP_BS
    h->stat.frame.i_tex_bits += x264_vp8rac_pos( cb ) - i_mb_pos_tex;
#endif
}
