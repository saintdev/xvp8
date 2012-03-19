/*****************************************************************************
 * ivf.c: ivf muxer
 *****************************************************************************
 * Copyright (C) 2009-2012 x264 project
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

#include "output.h"
#include "flv_bytestream.h"

#define CHECK(x)\
do {\
    if( (x) < 0 )\
        return -1;\
} while( 0 )

typedef struct
{
    flv_buffer *p_buf;

    int64_t i_framenum;

    uint64_t i_framenum_pos;
} ivf_hnd_t;

static int open_file( char *psz_filename, hnd_t *p_handle, cli_output_opt_t *opt )
{
    ivf_hnd_t *p_ivf = calloc( 1, sizeof(*p_ivf) );
    *p_handle = NULL;
    if( !p_ivf )
        return -1;

    p_ivf->p_buf = flv_create_writer( psz_filename );
    if( !p_ivf->p_buf )
        return -1;

    *p_handle = p_ivf;

    return 0;
}

static int set_param( hnd_t handle, x264_param_t *p_param )
{
    ivf_hnd_t *p_ivf = handle;
    flv_buffer *p_buf = p_ivf->p_buf;

    flv_put_tag( p_buf, "DKIF" );   // signature
    flv_put_le16( p_buf, 0 );       // version
    flv_put_le16( p_buf, 32 );      // header size in bytes
    flv_put_tag( p_buf, "VP80" );   // codec FourCC
    flv_put_le16( p_buf, p_param->i_width );
    flv_put_le16( p_buf, p_param->i_height );
    /* FIXME: VFR */
    flv_put_le32( p_buf, p_param->i_fps_num );
    flv_put_le32( p_buf, p_param->i_fps_den );
    p_ivf->i_framenum_pos = p_buf->d_cur + p_buf->d_total + 1;
    flv_put_le32( p_buf, 0 );       // written on close
    flv_put_le32( p_buf, 0 );       // unused

    CHECK( flv_flush_data( p_buf ) );

    return 0;
}

static int write_headers( hnd_t handle, x264_nal_t *p_nal )
{
    return 0;
}

static int write_frame( hnd_t handle, uint8_t *p_nalu, int i_size, x264_picture_t *p_picture )
{
    ivf_hnd_t *p_ivf = handle;
    flv_buffer *p_buf = p_ivf->p_buf;

    flv_put_le32( p_buf, i_size );  // size of frame not including header
    /* FIXME: VFR */
    flv_put_le64( p_buf, p_picture->i_pts );

    flv_append_data( p_buf, p_nalu, i_size );

    CHECK( flv_flush_data( p_buf ) );

    p_ivf->i_framenum++;

    return i_size;
}

static void rewrite_le32( FILE *fp, uint64_t position, uint32_t value )
{
    fseek( fp, position, SEEK_SET );
    fwrite( &value, 4, 1, fp );
}

static int close_file( hnd_t handle, int64_t largest_pts, int64_t second_largest_pts )
{
    ivf_hnd_t *p_ivf = handle;
    flv_buffer *p_buf = p_ivf->p_buf;

    CHECK( flv_flush_data( p_buf ) );

    rewrite_le32( p_buf->fp, p_ivf->i_framenum_pos, p_ivf->i_framenum );

    fclose( p_buf->fp );
    free( p_ivf );
    free( p_buf );

    return 0;
}

const cli_output_t ivf_output = { open_file, set_param, write_headers, write_frame, close_file };
