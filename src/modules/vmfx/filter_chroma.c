/*
 * filter_chroma.c -- Maps a chroma key to the alpha channel
 * Copyright (C) 2005 Visual Media Fx Inc.
 * Author: Charles Yates <charles.yates@pandora.be>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "filter_chroma.h"
#include <stdlib.h>
#include <framework/mlt_factory.h>
#include <framework/mlt_frame.h>
#include <framework/mlt_producer.h>
#include <framework/mlt_geometry.h>

static inline int in_range( uint8_t v, uint8_t c, int var )
{
	return ( ( int )v >= c - var ) && ( ( int )v <= c + var );
}

static inline uint8_t alpha_value( uint8_t a, uint8_t *p, uint8_t u, uint8_t v, int var )
{
	return ( in_range( *( p + 1 ), u, var ) && in_range( *( p + 3 ), v, var ) ) ? 0 : a;
}

/** Get the images and map the chroma to the alpha of the frame.
*/

static int filter_get_image( mlt_frame frame, uint8_t **image, mlt_image_format *format, int *width, int *height, int writable )
{
	mlt_filter this = mlt_frame_pop_service( frame );
	char *key = mlt_properties_get( MLT_FILTER_PROPERTIES( this ), "key" );
	int variance = 200 * mlt_properties_get_double( MLT_FILTER_PROPERTIES( this ), "variance" );
	int32_t key_val = strtol( key, &key, 0 );
	uint8_t b = key_val & 0xff;
	uint8_t g = ( key_val >> 8 ) & 0xff;
	uint8_t r = ( key_val >> 16 ) & 0xff;
	uint8_t y, u, v;

	RGB2YUV( r, g, b, y, u, v );

	if ( mlt_frame_get_image( frame, image, format, width, height, writable ) == 0 )
	{
		uint8_t *alpha = mlt_frame_get_alpha_mask( frame );
		uint8_t *p = *image;
		int size = *width * *height / 2;
		while ( size -- )
		{
			*alpha = alpha_value( *alpha, p, u, v, variance );
			*alpha ++;
			p += 2;
			*alpha = alpha_value( *alpha, p, v, u, variance );
			alpha ++;
			p += 2;
		}
	}

	return 0;
}

/** Filter processing.
*/

static mlt_frame filter_process( mlt_filter this, mlt_frame frame )
{
	mlt_frame_push_service( frame, this );
	mlt_frame_push_service( frame, filter_get_image );
	return frame;
}

/** Constructor for the filter.
*/

mlt_filter filter_chroma_init( char *arg )
{
	mlt_filter this = mlt_filter_new( );
	if ( this != NULL )
	{
		mlt_properties_set( MLT_FILTER_PROPERTIES( this ), "key", arg == NULL ? "0x00ff00" : arg );
		mlt_properties_set_double( MLT_FILTER_PROPERTIES( this ), "variance", 0.3 );
		this->process = filter_process;
	}
	return this;
}
