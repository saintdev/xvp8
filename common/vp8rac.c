/*****************************************************************************
 * vp8rac.c: vp8 arithmetic coder
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
 * This file uses BSD-licensed code from Google's libvpx.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at licensing@x264.com.
 *****************************************************************************/

#include "common.h"

static const uint8_t x264_vp8rac_renorm_shift[256] =
{
    0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*****************************************************************************
 *
 *****************************************************************************/
void x264_vp8rac_encode_init_core( x264_cabac_t *cb )
{
    cb->i_low   = 0;
    cb->i_range = 255;
    cb->i_queue = -9;
    cb->i_bytes_outstanding = 0;
}

void x264_vp8rac_encode_init( x264_cabac_t *cb, uint8_t *p_data, uint8_t *p_end )
{
    x264_vp8rac_encode_init_core( cb );
    cb->p_start = p_data;
    cb->p       = p_data;
    cb->p_end   = p_end;
}

static inline void x264_vp8rac_putbyte( x264_cabac_t *cb )
{
    if( cb->i_queue >= 0 )
    {
        int out = cb->i_low >> (cb->i_queue+10);
        cb->i_low &= (0x400<<cb->i_queue)-1;
        cb->i_queue -= 8;

        if( (out & 0xff) == 0xff )
            cb->i_bytes_outstanding++;
        else
        {
            int carry = out >> 8;
            int bytes_outstanding = cb->i_bytes_outstanding;
            // this can't modify before the beginning of the stream because
            // that would correspond to a probability > 1.
            // it will write before the beginning of the stream, which is ok
            // because a slice header always comes before cabac data.
            // this can't carry beyond the one byte, because any 0xff bytes
            // are in bytes_outstanding and thus not written yet.
            cb->p[-1] += carry;
            while( bytes_outstanding > 0 )
            {
                *(cb->p++) = carry-1;
                bytes_outstanding--;
            }
            *(cb->p++) = out;
            cb->i_bytes_outstanding = 0;
        }
    }
}

static inline void x264_vp8rac_encode_renorm( x264_cabac_t *cb )
{
    int shift = x264_vp8rac_renorm_shift[cb->i_range];
    cb->i_range <<= shift;
    cb->i_low   <<= shift;
    cb->i_queue  += shift;
    x264_vp8rac_putbyte( cb );
}

void x264_vp8rac_encode_decision( x264_cabac_t *cb, int prob, int b )
{
    int i_range_lps = 1 + (((cb->i_range-1) * prob)>>8);
    cb->i_range -= i_range_lps;
    if( !b )
    {
        cb->i_low += cb->i_range;
        cb->i_range = i_range_lps;
    }
    x264_vp8rac_encode_renorm( cb );
}

void x264_vp8rac_encode_bypass( x264_cabac_t *cb, int b )
{
    x264_vp8rac_encode_decision( cb, 0x80, b );
}

void x264_vp8rac_encode_uint_bypass( x264_cabac_t *cb, int val, int bits )
{
    for( ; bits >= 0; bits-- )
        x264_vp8rac_encode_decision( cb, 0x80, (val>>(bits-1))&1 );
}

void x264_vp8rac_encode_flush( x264_t *h, x264_cabac_t *cb )
{
    for( int i = 0; i < 32; i++ )
        x264_vp8rac_encode_decision( cb, 0x80, 0 );
}

