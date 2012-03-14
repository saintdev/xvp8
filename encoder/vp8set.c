/*****************************************************************************
 * vp8set.c: vp8 header writing
 *****************************************************************************
 * Copyright (C) 2003-2012 x264 project
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Loren Merritt <lorenm@u.washington.edu>
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
#include "set.h"

void x264_vp8_slice_header_write( x264_t *h, bs_t *s, x264_slice_header_t *sh )
{
    int keyframe = sh->i_idr_pic_id >= 0;
    int header_byte = (keyframe                << 0)
                    + (0 /* regular profile */ << 1)
                    + (0 /* not invisible */   << 4);
    /* I'm making up a number now for testing. */
    int header_size = 30;
    int header = (header_size << 5) + header_byte;
    /* Stupid little-endian headers */
    bs_write( s, 8, (header>> 0)&0xff );
    bs_write( s, 8, (header>> 8)&0xff );
    bs_write( s, 8, (header>>16)      );

    if( keyframe )
    {
        /* Startcode */
        bs_write( s, 24, 0x9d012a );

        /* Fixme: make sure to cap resolutions at 0x3fff to avoid hscale/vscale */
        bs_write( s, 8, h->param.i_width&0xff  );
        bs_write( s, 8, h->param.i_width>>8    );
        bs_write( s, 8, h->param.i_height&0xff );
        bs_write( s, 8, h->param.i_height>>8   );
    }
        
    x264_vp8rac_encode_init( &h->cabac, h->out.bs.p, h->out.bs.p_end );
    if( keyframe )
    {
        x264_vp8rac_encode_bypass( &h->cabac, 0 ); /* colorspace */
        x264_vp8rac_encode_bypass( &h->cabac, 0 ); /* don't skip DSP clamping */
    }

    x264_vp8rac_encode_bypass( &h->cabac, 0 ); /* no segmentation for now */
    x264_vp8rac_encode_bypass( &h->cabac, 1 ); /* simple filter for now */
    x264_vp8rac_encode_uint_bypass( &h->cabac, 0, 6 ); /* filter level: 0 */
    x264_vp8rac_encode_uint_bypass( &h->cabac, 0, 3 ); /* filter sharpness: 0 */
    x264_vp8rac_encode_bypass( &h->cabac, 0 ); /* no lf deltas */
    x264_vp8rac_encode_uint_bypass( &h->cabac, 0, 2 ); /* No bitstream partitions */
    
    /* FIXME: use real quantizers */
    int base_qi = 20;
    x264_vp8rac_encode_uint_bypass( &h->cabac, base_qi, 7 ); /* yac_qi */
    x264_vp8rac_encode_sint_bypass( &h->cabac, 0, 4 );       /* ydc_delta */
    x264_vp8rac_encode_sint_bypass( &h->cabac, 0, 4 );       /* y2dc_delta */
    x264_vp8rac_encode_sint_bypass( &h->cabac, 0, 4 );       /* y2ac_delta */
    x264_vp8rac_encode_sint_bypass( &h->cabac, 0, 4 );       /* uvdc_delta */
    x264_vp8rac_encode_sint_bypass( &h->cabac, 0, 4 );       /* uvac_delta */
    
    if( !keyframe )
    {
        x264_vp8rac_encode_bypass( &h->cabac, 0 ); /* Don't update golden */
        x264_vp8rac_encode_bypass( &h->cabac, 0 ); /* Don't update altref */
        x264_vp8rac_encode_bypass( &h->cabac, 0 ); /* No golden sign bias */
        x264_vp8rac_encode_bypass( &h->cabac, 0 ); /* No altref sign bias */
    }
    
    /* TODO: frame probability updates, these are rather scary and will require
     * that we add actual probability tables. */
}
