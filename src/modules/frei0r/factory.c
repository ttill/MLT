/*
 * factory.c -- the factory method interfaces
 * Copyright (c) 2008 Marco Gittler <g.marco@freenet.de>
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
#include <frei0r.h>

#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>


#ifdef WIN32
#define LIBSUF ".dll"
#define FREI0R_PLUGIN_PATH "lib\\frei0r-1"
#else
#define LIBSUF ".so"
#define FREI0R_PLUGIN_PATH "/usr/lib/frei0r-1:/usr/lib64/frei0r-1:/opt/local/lib/frei0r-1:/usr/local/lib/frei0r-1:$HOME/.frei0r-1/lib"
#endif

#define GET_FREI0R_PATH (getenv("FREI0R_PATH") ? getenv("FREI0R_PATH") : getenv("MLT_FREI0R_PLUGIN_PATH") ? getenv("MLT_FREI0R_PLUGIN_PATH") : FREI0R_PLUGIN_PATH)

extern mlt_filter filter_frei0r_init( mlt_profile profile, mlt_service_type type, const char *id, char *arg );
extern mlt_frame filter_process( mlt_filter this, mlt_frame frame );
extern void filter_close( mlt_filter this );
extern int producer_get_frame( mlt_producer producer, mlt_frame_ptr frame, int index );
extern void producer_close( mlt_producer this );
extern void transition_close( mlt_transition this );
extern mlt_frame transition_process( mlt_transition transition, mlt_frame a_frame, mlt_frame b_frame );

static void check_thread_safe( mlt_properties properties, const char *name )
{
	char dirname[PATH_MAX];
	snprintf( dirname, PATH_MAX, "%s/frei0r/not_thread_safe.txt", mlt_environment( "MLT_DATA" ) );
	mlt_properties not_thread_safe = mlt_properties_load( dirname );
	int i;

	for ( i = 0; i < mlt_properties_count( not_thread_safe ); i++ )
	{
		if ( strcmp( name, mlt_properties_get_name( not_thread_safe, i ) ) == 0 )
		{
			mlt_properties_set_int( properties, "_not_thread_safe", 1 );
			break;
		}
	}
	mlt_properties_close( not_thread_safe );
}

static mlt_properties fill_param_info ( mlt_service_type type, const char *service_name, char *name )
{
	char file[ PATH_MAX ];
	char servicetype[ 1024 ]="";
	struct stat stat_buff;

	switch ( type ) {
		case producer_type:
			strcpy ( servicetype , "producer" );
			break;
		case filter_type:
			strcpy ( servicetype , "filter" );
			break;
		case transition_type:
			strcpy ( servicetype , "transition" ) ;
			break;
		default:
			strcpy ( servicetype , "" );
	};

	snprintf( file, PATH_MAX, "%s/frei0r/%s_%s.yml", mlt_environment( "MLT_DATA" ), servicetype, service_name );
	memset(&stat_buff, 0, sizeof(stat_buff));
	stat(file,&stat_buff);

	if (S_ISREG(stat_buff.st_mode)){
		return mlt_properties_parse_yaml( file );
	}

	void* handle=dlopen(name,RTLD_LAZY);
	if (!handle) return NULL;
	void (*plginfo)(f0r_plugin_info_t*)=dlsym(handle,"f0r_get_plugin_info");
	void (*param_info)(f0r_param_info_t*,int param_index)=dlsym(handle,"f0r_get_param_info");
	void (*f0r_init)(void)=dlsym(handle,"f0r_init");
	void (*f0r_deinit)(void)=dlsym(handle,"f0r_deinit");
	if (!plginfo || !param_info) {
		dlclose(handle);
		return NULL;
	}
	mlt_properties metadata = mlt_properties_new();
	f0r_plugin_info_t info;
	char string[48];
	int j=0;

	f0r_init();
	plginfo(&info);
	snprintf ( string, sizeof(string) , "%d" , info.minor_version );
	mlt_properties_set_double ( metadata, "schema_version" , 0.1 );
	mlt_properties_set ( metadata, "title" , info.name );
	mlt_properties_set_double ( metadata, "version",
		info.major_version +  info.minor_version / pow( 10, strlen( string ) ) );
	mlt_properties_set ( metadata, "identifier" , service_name );
	mlt_properties_set ( metadata, "description" , info.explanation );
	mlt_properties_set ( metadata, "creator" , info.author );
	switch (type){
		case producer_type:
			mlt_properties_set ( metadata, "type" , "producer" );
			break;
		case filter_type:
			mlt_properties_set ( metadata, "type" , "filter" );
			break;
		case transition_type:
			mlt_properties_set ( metadata, "type" , "transition" );
			break;
		default:
			break;
	}

	mlt_properties parameter = mlt_properties_new ( );
	mlt_properties_set_data ( metadata , "parameters" , parameter , 0 , ( mlt_destructor )mlt_properties_close, NULL );
	mlt_properties tags = mlt_properties_new ( );
	mlt_properties_set_data ( metadata , "tags" , tags , 0 , ( mlt_destructor )mlt_properties_close, NULL );
	mlt_properties_set ( tags , "0" , "Video" );

	for (j=0;j<info.num_params;j++){
		snprintf ( string , sizeof(string), "%d" , j );
		mlt_properties pnum = mlt_properties_new ( );
		mlt_properties_set_data ( parameter , string , pnum , 0 , ( mlt_destructor )mlt_properties_close, NULL );
		f0r_param_info_t paraminfo;
		param_info(&paraminfo,j);
		mlt_properties_set ( pnum , "identifier" , paraminfo.name );
		mlt_properties_set ( pnum , "title" , paraminfo.name );
		mlt_properties_set ( pnum , "description" , paraminfo.explanation);
		if ( paraminfo.type == F0R_PARAM_DOUBLE ){
			mlt_properties_set ( pnum , "type" , "float" );
			mlt_properties_set ( pnum , "minimum" , "0" );
			mlt_properties_set ( pnum , "maximum" , "1" );
			mlt_properties_set ( pnum , "readonly" , "no" );
			mlt_properties_set ( pnum , "widget" , "spinner" );
		}else
		if ( paraminfo.type == F0R_PARAM_BOOL ){
			mlt_properties_set ( pnum , "type" , "boolean" );
			mlt_properties_set ( pnum , "minimum" , "0" );
			mlt_properties_set ( pnum , "maximum" , "1" );
			mlt_properties_set ( pnum , "readonly" , "no" );
		}else
		if ( paraminfo.type == F0R_PARAM_COLOR ){
			mlt_properties_set ( pnum , "type" , "color" );
			mlt_properties_set ( pnum , "readonly" , "no" );
		}else
		if ( paraminfo.type == F0R_PARAM_STRING ){
			mlt_properties_set ( pnum , "type" , "string" );
			mlt_properties_set ( pnum , "readonly" , "no" );
		}
	}
	f0r_deinit();
	dlclose(handle);
	free(name);

	return metadata;
}

static void * load_lib( mlt_profile profile, mlt_service_type type , void* handle, const char *name ){

	int i=0;
	void (*f0r_get_plugin_info)(f0r_plugin_info_t*),
		*f0r_construct , *f0r_update , *f0r_destruct,
		(*f0r_get_param_info)(f0r_param_info_t* info, int param_index),
		(*f0r_init)(void) , *f0r_deinit ,
		(*f0r_set_param_value)(f0r_instance_t instance, f0r_param_t param, int param_index),
		(*f0r_get_param_value)(f0r_instance_t instance, f0r_param_t param, int param_index),
		(*f0r_update2) (f0r_instance_t instance, double time,	const uint32_t* inframe1,
		  const uint32_t* inframe2,const uint32_t* inframe3, uint32_t* outframe);

	if ( ( f0r_construct = dlsym(handle, "f0r_construct") ) &&
				(f0r_destruct = dlsym(handle,"f0r_destruct") ) &&
				(f0r_get_plugin_info = dlsym(handle,"f0r_get_plugin_info") ) &&
				(f0r_get_param_info = dlsym(handle,"f0r_get_param_info") ) &&
				(f0r_set_param_value= dlsym(handle,"f0r_set_param_value" ) ) &&
				(f0r_get_param_value= dlsym(handle,"f0r_get_param_value" ) ) &&
				(f0r_init= dlsym(handle,"f0r_init" ) ) &&
				(f0r_deinit= dlsym(handle,"f0r_deinit" ) )
		){

		f0r_update=dlsym(handle,"f0r_update");
		f0r_update2=dlsym(handle,"f0r_update2");

		f0r_plugin_info_t info;
		f0r_get_plugin_info(&info);

		void* ret=NULL;
		mlt_properties properties=NULL;
		char minor[12];

		if (type == producer_type && info.plugin_type == F0R_PLUGIN_TYPE_SOURCE ){
			mlt_producer this = mlt_producer_new( profile );
			if ( this != NULL )
			{
				this->get_frame = producer_get_frame;
				this->close = ( mlt_destructor )producer_close;
				f0r_init();
				properties=MLT_PRODUCER_PROPERTIES ( this );

				for (i=0;i<info.num_params;i++){
					f0r_param_info_t pinfo;
					f0r_get_param_info(&pinfo,i);

				}

				ret=this;
			}
		} else if (type == filter_type && info.plugin_type == F0R_PLUGIN_TYPE_FILTER ){
			mlt_filter this = mlt_filter_new( );
			if ( this != NULL )
			{
				this->process = filter_process;
				this->close = filter_close;
				f0r_init();
				properties=MLT_FILTER_PROPERTIES ( this );

				for (i=0;i<info.num_params;i++){
					f0r_param_info_t pinfo;
					f0r_get_param_info(&pinfo,i);

				}

				ret=this;
			}
		}else if (type == transition_type && info.plugin_type == F0R_PLUGIN_TYPE_MIXER2){
			mlt_transition transition = mlt_transition_new( );
			if ( transition != NULL )
			{
				transition->process = transition_process;
				transition->close = transition_close;
				properties=MLT_TRANSITION_PROPERTIES( transition );
				mlt_properties_set_int(properties, "_transition_type", 1 );

				ret=transition;
			}
		}
		check_thread_safe( properties, name );
		mlt_properties_set_data(properties, "_dlclose_handle", handle , sizeof (void*) , NULL , NULL );
		mlt_properties_set_data(properties, "_dlclose", dlclose , sizeof (void*) , NULL , NULL );
		mlt_properties_set_data(properties, "f0r_construct", f0r_construct , sizeof(void*),NULL,NULL);
		mlt_properties_set_data(properties, "f0r_update", f0r_update , sizeof(void*),NULL,NULL);
		if (f0r_update2)
			mlt_properties_set_data(properties, "f0r_update2", f0r_update2 , sizeof(void*),NULL,NULL);
		mlt_properties_set_data(properties, "f0r_destruct", f0r_destruct , sizeof(void*),NULL,NULL);
		mlt_properties_set_data(properties, "f0r_get_plugin_info", f0r_get_plugin_info , sizeof(void*),NULL,NULL);
		mlt_properties_set_data(properties, "f0r_get_param_info", f0r_get_param_info , sizeof(void*),NULL,NULL);
		mlt_properties_set_data(properties, "f0r_set_param_value", f0r_set_param_value , sizeof(void*),NULL,NULL);
		mlt_properties_set_data(properties, "f0r_get_param_value", f0r_get_param_value , sizeof(void*),NULL,NULL);

		// Let frei0r plugin version be serialized using same format as metadata
		snprintf( minor, sizeof( minor ), "%d", info.minor_version );
		mlt_properties_set_double( properties, "version", info.major_version +  info.minor_version / pow( 10, strlen( minor ) ) );

		return ret;
	}else{
		mlt_log_error( NULL, "frei0r plugin \"%s\" is missing a function\n", name );
		dlerror();
	}
	return NULL;
}

static void * create_frei0r_item ( mlt_profile profile, mlt_service_type type, const char *id, void *arg){

	mlt_tokeniser tokeniser = mlt_tokeniser_init ( );
	int dircount=mlt_tokeniser_parse_new (
		tokeniser,
		GET_FREI0R_PATH,
		 ":"
	);
	void* ret=NULL;
	while (dircount--){
		char soname[PATH_MAX];
		char *myid = strdup( id );

#ifdef WIN32
		char *firstname = strtok( myid, "." );
#else
		char *save_firstptr = NULL;
		char *firstname = strtok_r( myid, ".", &save_firstptr );
#endif
		char* directory = mlt_tokeniser_get_string (tokeniser, dircount);

#ifdef WIN32
		firstname = strtok( NULL, "." );
#else
		firstname = strtok_r( NULL, ".", &save_firstptr );
#endif
		if (strncmp(directory, "$HOME", 5))
			snprintf(soname, PATH_MAX, "%s/%s" LIBSUF, directory, firstname );
		else
			snprintf(soname, PATH_MAX, "%s%s/%s" LIBSUF, getenv("HOME"), strchr(directory, '/'), firstname );

		if (firstname){

			void* handle=dlopen(soname,RTLD_LAZY);

			if (handle ){
				ret=load_lib ( profile , type , handle, firstname );
			}else{
				dlerror();
			}
		}
		free( myid );
	}
	mlt_tokeniser_close ( tokeniser );
	return ret;
}


MLT_REPOSITORY
{
	int i=0;
	mlt_tokeniser tokeniser = mlt_tokeniser_init ( );
	int dircount=mlt_tokeniser_parse_new (
		tokeniser ,
		GET_FREI0R_PATH,
		":"
	);
	char dirname[PATH_MAX];
	snprintf( dirname, PATH_MAX, "%s/frei0r/blacklist.txt", mlt_environment( "MLT_DATA" ) );
	mlt_properties blacklist = mlt_properties_load( dirname );

	while (dircount--){

		mlt_properties direntries = mlt_properties_new();
		char* directory = mlt_tokeniser_get_string (tokeniser, dircount);
		
		if (strncmp(directory, "$HOME", 5))
			snprintf(dirname, PATH_MAX, "%s", directory);
		else
			snprintf(dirname, PATH_MAX, "%s%s", getenv("HOME"), strchr(directory, '/'));
		mlt_properties_dir_list(direntries, dirname ,"*" LIBSUF, 1);

		for (i=0; i<mlt_properties_count(direntries);i++){
			char* name=mlt_properties_get_value(direntries,i);
			char* shortname=name+strlen(dirname)+1;
#ifdef WIN32
			char* firstname = strtok( shortname, "." );
#else
			char *save_firstptr = NULL;
			char* firstname = strtok_r( shortname, ".", &save_firstptr );
#endif
			char pluginname[1024]="frei0r.";
			strcat(pluginname,firstname);

			if ( mlt_properties_get( blacklist, firstname ) )
				continue;

			void* handle=dlopen(strcat(name, LIBSUF),RTLD_LAZY);
			if (handle){
				void (*plginfo)(f0r_plugin_info_t*)=dlsym(handle,"f0r_get_plugin_info");

				if (plginfo){
					f0r_plugin_info_t info;
					plginfo(&info);
					if (firstname && info.plugin_type==F0R_PLUGIN_TYPE_SOURCE){
						if (mlt_properties_get(mlt_repository_producers(repository), pluginname))
						{
							dlclose(handle);
							continue;
						}
						MLT_REGISTER( producer_type, pluginname, create_frei0r_item );
						MLT_REGISTER_METADATA( producer_type, pluginname, fill_param_info, strdup(name) );
					}
					else if (firstname && info.plugin_type==F0R_PLUGIN_TYPE_FILTER){
						if (mlt_properties_get(mlt_repository_filters(repository), pluginname))
						{
							dlclose(handle);
							continue;
						}
						MLT_REGISTER( filter_type, pluginname, create_frei0r_item );
						MLT_REGISTER_METADATA( filter_type, pluginname, fill_param_info, strdup(name) );
					}
					else if (firstname && info.plugin_type==F0R_PLUGIN_TYPE_MIXER2 ){
						if (mlt_properties_get(mlt_repository_transitions(repository), pluginname))
						{
							dlclose(handle);
							continue;
						}
						MLT_REGISTER( transition_type, pluginname, create_frei0r_item );
						MLT_REGISTER_METADATA( transition_type, pluginname, fill_param_info, strdup(name) );
					}
				}
				dlclose(handle);
			}
		}
		mlt_properties_close(direntries);
	}
	mlt_tokeniser_close ( tokeniser );
	mlt_properties_close( blacklist );
}
