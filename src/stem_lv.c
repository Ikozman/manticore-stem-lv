/*
 * Manticore Search token filter plugins for Latvian stemming.
 *
 * Library:  stem_lv.so (Linux) / stem_lv.dll (Windows)
 *
 * Index-time filter "stem_lv":
 *     index_token_filter = 'stem_lv.so:stem_lv:mode=both'
 *   mode=both     (default) index the word as is plus its stem at the same position
 *   mode=replace  index only the stem
 *
 * Query-time filter "stem_lv_query":
 *     SELECT ... WHERE MATCH('televizoru') OPTION token_filter='stem_lv.so:stem_lv_query:'
 *   replaces every query word with its stem.
 *
 * Licensed under the Apache License, Version 2.0. See LICENSE and NOTICE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "latvian_stemmer.h"

/*
 * Plugin ABI version this library was built against (SPH_UDF_VERSION from
 * Manticore's src/sphinxudf.h). searchd accepts a library whose version is
 * the same or newer than its own.
 */
#define STEM_LV_UDF_VERSION 12

#define STEM_LV_ERROR_LEN 256

/* A stem is never longer than the word in bytes; 4 bytes per code point is the UTF-8 ceiling. */
#define STEM_LV_BUFFER_SIZE ( LV_STEM_MAX_CODEPOINTS * 4 + 1 )

#if defined(_WIN32)
#define STEM_LV_EXPORT __declspec(dllexport)
#else
#define STEM_LV_EXPORT __attribute__((visibility("default")))
#endif

STEM_LV_EXPORT int stem_lv_ver ( void )
{
	return STEM_LV_UDF_VERSION;
}

/* ---------------------------------------------------------------------- */
/* index-time token filter                                                 */
/* ---------------------------------------------------------------------- */

typedef struct
{
	int replace;
	int has_extra;
	char stem[STEM_LV_BUFFER_SIZE];
} stem_lv_index_state;

static int parse_index_options ( const char * options, stem_lv_index_state * state, char * error_message )
{
	const char * mode;

	state->replace = 0;

	if ( !options || !*options )
		return 0;

	mode = strstr ( options, "mode=" );
	if ( !mode )
		return 0;

	mode += 5;
	if ( strncmp ( mode, "replace", 7 ) == 0 && ( mode[7] == '\0' || mode[7] == ';' ) )
		state->replace = 1;
	else if ( !( strncmp ( mode, "both", 4 ) == 0 && ( mode[4] == '\0' || mode[4] == ';' ) ) )
	{
		snprintf ( error_message, STEM_LV_ERROR_LEN, "stem_lv: unknown mode in '%s', expected mode=both or mode=replace", options );
		return 1;
	}

	return 0;
}

STEM_LV_EXPORT int stem_lv_init ( void ** userdata, int num_fields, const char ** field_names, const char * options, char * error_message )
{
	stem_lv_index_state * state = (stem_lv_index_state *) calloc ( 1, sizeof ( stem_lv_index_state ) );

	(void) num_fields;
	(void) field_names;

	if ( !state )
	{
		snprintf ( error_message, STEM_LV_ERROR_LEN, "stem_lv: out of memory" );
		return 1;
	}

	if ( parse_index_options ( options, state, error_message ) != 0 )
	{
		free ( state );
		return 1;
	}

	*userdata = state;
	return 0;
}

STEM_LV_EXPORT int stem_lv_begin_document ( void * userdata, const char * options, char * error_message )
{
	(void) userdata;
	(void) options;
	(void) error_message;
	return 0;
}

STEM_LV_EXPORT void stem_lv_begin_field ( void * userdata, int field_index )
{
	stem_lv_index_state * state = (stem_lv_index_state *) userdata;

	(void) field_index;
	if ( state )
		state->has_extra = 0;
}

STEM_LV_EXPORT char * stem_lv_push_token ( void * userdata, char * token, int * extra, int * delta )
{
	stem_lv_index_state * state = (stem_lv_index_state *) userdata;

	(void) delta; /* keep the position delta computed by searchd */
	*extra = 0;

	if ( !state || !token || !lv_stem_utf8 ( token, state->stem, sizeof ( state->stem ) ) )
		return token;

	if ( state->replace )
		return state->stem;

	/* both: emit the word now, its stem next at the same position */
	state->has_extra = 1;
	*extra = 1;
	return token;
}

STEM_LV_EXPORT char * stem_lv_get_extra_token ( void * userdata, int * delta )
{
	stem_lv_index_state * state = (stem_lv_index_state *) userdata;

	*delta = 0;

	if ( !state || !state->has_extra )
		return NULL;

	state->has_extra = 0;
	return state->stem;
}

STEM_LV_EXPORT int stem_lv_end_field ( void * userdata )
{
	(void) userdata;
	return 0; /* no tokens are held back until the end of a field */
}

STEM_LV_EXPORT void stem_lv_deinit ( void * userdata )
{
	free ( userdata );
}

/* ---------------------------------------------------------------------- */
/* query-time token filter                                                 */
/* ---------------------------------------------------------------------- */

typedef struct
{
	char stem[STEM_LV_BUFFER_SIZE];
} stem_lv_query_state;

STEM_LV_EXPORT int stem_lv_query_init ( void ** userdata, int max_len, const char * options, char * error_message )
{
	stem_lv_query_state * state = (stem_lv_query_state *) calloc ( 1, sizeof ( stem_lv_query_state ) );

	(void) max_len;
	(void) options;

	if ( !state )
	{
		snprintf ( error_message, STEM_LV_ERROR_LEN, "stem_lv_query: out of memory" );
		return 1;
	}

	*userdata = state;
	return 0;
}

STEM_LV_EXPORT char * stem_lv_query_push_token ( void * userdata, char * token, int * delta, const char * raw_token_start, int raw_token_len )
{
	stem_lv_query_state * state = (stem_lv_query_state *) userdata;

	(void) delta;
	(void) raw_token_start;
	(void) raw_token_len;

	if ( !state || !token || !lv_stem_utf8 ( token, state->stem, sizeof ( state->stem ) ) )
		return token;

	return state->stem;
}

STEM_LV_EXPORT void stem_lv_query_deinit ( void * userdata )
{
	free ( userdata );
}
