/*
 * Latvian light stemmer (C port of Apache Lucene's LatvianStemmer).
 *
 * Licensed under the Apache License, Version 2.0. See LICENSE and NOTICE.
 */

#ifndef LATVIAN_STEMMER_H
#define LATVIAN_STEMMER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Words longer than this are left unstemmed. */
#define LV_STEM_MAX_CODEPOINTS 128

/*
 * Stems a lowercase word given as Unicode code points, in place.
 * Returns the new length. The array must have room for len + 1 items.
 */
int lv_stem_codepoints ( unsigned int * s, int len );

/*
 * Stems a lowercase UTF-8 word into out.
 * Returns 1 when out holds a stem that differs from the word, 0 otherwise
 * (word unchanged, not a plain word, invalid UTF-8 or out too small).
 */
int lv_stem_utf8 ( const char * word, char * out, int out_size );

#ifdef __cplusplus
}
#endif

#endif
