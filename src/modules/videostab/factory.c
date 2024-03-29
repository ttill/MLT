/*
 * factory.c -- the factory method interfaces
 * Copyright (c) 2011 Marco Gittler <g.marco@freenet.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include <framework/mlt.h>
#include <limits.h>

extern mlt_filter filter_videostab_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg );

static mlt_properties videostab_metadata( mlt_service_type type, const char *id, void *data )
{
	char file[ PATH_MAX ];
	snprintf( file, PATH_MAX, "%s/videostab/filter_%s.yml", mlt_environment( "MLT_DATA" ), id );
	return mlt_properties_parse_yaml( file );
}

MLT_REPOSITORY
{
	MLT_REGISTER( filter_type, "videostab", filter_videostab_init );
	MLT_REGISTER_METADATA( filter_type, "videostab", videostab_metadata, NULL );
	
}



