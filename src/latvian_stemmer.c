/*
 * Latvian light stemmer.
 *
 * C port of org.apache.lucene.analysis.lv.LatvianStemmer (Apache Lucene),
 * which is a light version of the algorithm in Karlis Kreslins' PhD thesis
 * "A stemming algorithm for Latvian".
 *
 * Licensed under the Apache License, Version 2.0. See LICENSE and NOTICE.
 */

#include "latvian_stemmer.h"

#include <string.h>

/* Latvian letters with diacritics, as Unicode code points. */
#define A_MACRON 0x0101 /* ā */
#define C_CARON  0x010D /* č */
#define E_MACRON 0x0113 /* ē */
#define I_MACRON 0x012B /* ī */
#define L_CEDIL  0x013C /* ļ */
#define N_CEDIL  0x0146 /* ņ */
#define S_CARON  0x0161 /* š */
#define U_MACRON 0x016B /* ū */
#define Z_CARON  0x017E /* ž */

typedef struct
{
	unsigned int affix[5];
	int length;
	int vowel_count;
	int palatalizes;
} lv_affix;

/* Order matters: the first matching suffix wins, exactly as in Lucene. */
static const lv_affix AFFIXES[] = {
	{ { 'a', 'j', 'i', 'e', 'm' }, 5, 3, 0 },
	{ { 'a', 'j', 'a', 'i' }, 4, 3, 0 },
	{ { 'a', 'j', 'a', 'm' }, 4, 2, 0 },
	{ { 'a', 'j', A_MACRON, 'm' }, 4, 2, 0 },
	{ { 'a', 'j', 'o', 's' }, 4, 2, 0 },
	{ { 'a', 'j', A_MACRON, 's' }, 4, 2, 0 },
	{ { 'i', 'e', 'm' }, 3, 2, 1 },
	{ { 'a', 'j', A_MACRON }, 3, 2, 0 },
	{ { 'a', 'i', 's' }, 3, 2, 0 },
	{ { 'a', 'i' }, 2, 2, 0 },
	{ { 'e', 'i' }, 2, 2, 0 },
	{ { A_MACRON, 'm' }, 2, 1, 0 },
	{ { 'a', 'm' }, 2, 1, 0 },
	{ { E_MACRON, 'm' }, 2, 1, 0 },
	{ { I_MACRON, 'm' }, 2, 1, 0 },
	{ { 'i', 'm' }, 2, 1, 0 },
	{ { 'u', 'm' }, 2, 1, 0 },
	{ { 'u', 's' }, 2, 1, 1 },
	{ { 'a', 's' }, 2, 1, 0 },
	{ { A_MACRON, 's' }, 2, 1, 0 },
	{ { 'e', 's' }, 2, 1, 0 },
	{ { 'o', 's' }, 2, 1, 1 },
	{ { 'i', 'j' }, 2, 1, 0 },
	{ { I_MACRON, 's' }, 2, 1, 0 },
	{ { E_MACRON, 's' }, 2, 1, 0 },
	{ { 'i', 's' }, 2, 1, 0 },
	{ { 'i', 'e' }, 2, 1, 0 },
	{ { 'u' }, 1, 1, 1 },
	{ { 'a' }, 1, 1, 1 },
	{ { 'i' }, 1, 1, 1 },
	{ { 'e' }, 1, 1, 0 },
	{ { A_MACRON }, 1, 1, 0 },
	{ { E_MACRON }, 1, 1, 0 },
	{ { I_MACRON }, 1, 1, 0 },
	{ { U_MACRON }, 1, 1, 0 },
	{ { 'o' }, 1, 1, 0 },
	{ { 's' }, 1, 0, 0 },
	{ { S_CARON }, 1, 0, 0 },
};

#define AFFIX_COUNT ( (int) ( sizeof ( AFFIXES ) / sizeof ( AFFIXES[0] ) ) )

static int ends_with ( const unsigned int * s, int len, const unsigned int * suffix, int suffix_len )
{
	int i;

	if ( len < suffix_len )
		return 0;

	for ( i = 0; i < suffix_len; i++ )
		if ( s[len - suffix_len + i] != suffix[i] )
			return 0;

	return 1;
}

static int ends_with2 ( const unsigned int * s, int len, unsigned int first, unsigned int second )
{
	return len >= 2 && s[len - 2] == first && s[len - 1] == second;
}

static int count_vowels ( const unsigned int * s, int len )
{
	int i, n = 0;

	for ( i = 0; i < len; i++ )
	{
		switch ( s[i] )
		{
		case 'a': case 'e': case 'i': case 'o': case 'u':
		case A_MACRON: case I_MACRON: case E_MACRON: case U_MACRON:
			n++;
		}
	}

	return n;
}

/*
 * Undo the consonant change a declension II, V or VI suffix caused.
 * The ambiguous s/t -> š and d/z -> ž cases are left alone, as in Lucene.
 * The caller guarantees capacity for one extra code point (kš -> kst).
 */
static int unpalatalize ( unsigned int * s, int len )
{
	/* The removed character is still in s[len]: -u means genitive plural. */
	if ( s[len] == 'u' )
	{
		if ( ends_with2 ( s, len, 'k', S_CARON ) )
		{
			len++;
			s[len - 2] = 's';
			s[len - 1] = 't';
			return len;
		}

		if ( ends_with2 ( s, len, N_CEDIL, N_CEDIL ) )
		{
			s[len - 2] = 'n';
			s[len - 1] = 'n';
			return len;
		}
	}

	if ( ends_with2 ( s, len, 'p', 'j' ) || ends_with2 ( s, len, 'b', 'j' )
		|| ends_with2 ( s, len, 'm', 'j' ) || ends_with2 ( s, len, 'v', 'j' ) )
		return len - 1;

	if ( ends_with2 ( s, len, S_CARON, N_CEDIL ) )
	{
		s[len - 2] = 's';
		s[len - 1] = 'n';
	}
	else if ( ends_with2 ( s, len, Z_CARON, N_CEDIL ) )
	{
		s[len - 2] = 'z';
		s[len - 1] = 'n';
	}
	else if ( ends_with2 ( s, len, S_CARON, L_CEDIL ) )
	{
		s[len - 2] = 's';
		s[len - 1] = 'l';
	}
	else if ( ends_with2 ( s, len, Z_CARON, L_CEDIL ) )
	{
		s[len - 2] = 'z';
		s[len - 1] = 'l';
	}
	else if ( ends_with2 ( s, len, L_CEDIL, N_CEDIL ) )
	{
		s[len - 2] = 'l';
		s[len - 1] = 'n';
	}
	else if ( ends_with2 ( s, len, L_CEDIL, L_CEDIL ) )
	{
		s[len - 2] = 'l';
		s[len - 1] = 'l';
	}
	else if ( len >= 1 && s[len - 1] == C_CARON )
		s[len - 1] = 'c';
	else if ( len >= 1 && s[len - 1] == L_CEDIL )
		s[len - 1] = 'l';
	else if ( len >= 1 && s[len - 1] == N_CEDIL )
		s[len - 1] = 'n';

	return len;
}

int lv_stem_codepoints ( unsigned int * s, int len )
{
	int i;
	int vowels = count_vowels ( s, len );

	for ( i = 0; i < AFFIX_COUNT; i++ )
	{
		const lv_affix * affix = &AFFIXES[i];

		if ( vowels > affix->vowel_count && len >= affix->length + 3
			&& ends_with ( s, len, affix->affix, affix->length ) )
		{
			len -= affix->length;
			return affix->palatalizes ? unpalatalize ( s, len ) : len;
		}
	}

	return len;
}

/* Decodes UTF-8 into code points; returns -1 on invalid input or overflow. */
static int utf8_decode ( const char * src, unsigned int * out, int max_out )
{
	const unsigned char * p = (const unsigned char *) src;
	int n = 0;

	while ( *p )
	{
		unsigned int c;
		int extra, k;

		if ( n >= max_out )
			return -1;

		if ( p[0] < 0x80 )
		{
			c = p[0];
			extra = 0;
		}
		else if ( ( p[0] & 0xE0 ) == 0xC0 )
		{
			c = p[0] & 0x1F;
			extra = 1;
		}
		else if ( ( p[0] & 0xF0 ) == 0xE0 )
		{
			c = p[0] & 0x0F;
			extra = 2;
		}
		else if ( ( p[0] & 0xF8 ) == 0xF0 )
		{
			c = p[0] & 0x07;
			extra = 3;
		}
		else
			return -1;

		for ( k = 1; k <= extra; k++ )
		{
			if ( ( p[k] & 0xC0 ) != 0x80 )
				return -1;
			c = ( c << 6 ) | ( p[k] & 0x3F );
		}

		out[n++] = c;
		p += extra + 1;
	}

	return n;
}

/* Encodes code points to UTF-8; returns bytes written or -1 when it does not fit. */
static int utf8_encode ( const unsigned int * s, int len, char * out, int out_size )
{
	int i, n = 0;

	for ( i = 0; i < len; i++ )
	{
		unsigned int c = s[i];
		int need = c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;

		if ( n + need >= out_size )
			return -1;

		switch ( need )
		{
		case 1:
			out[n++] = (char) c;
			break;
		case 2:
			out[n++] = (char) ( 0xC0 | ( c >> 6 ) );
			out[n++] = (char) ( 0x80 | ( c & 0x3F ) );
			break;
		case 3:
			out[n++] = (char) ( 0xE0 | ( c >> 12 ) );
			out[n++] = (char) ( 0x80 | ( ( c >> 6 ) & 0x3F ) );
			out[n++] = (char) ( 0x80 | ( c & 0x3F ) );
			break;
		default:
			out[n++] = (char) ( 0xF0 | ( c >> 18 ) );
			out[n++] = (char) ( 0x80 | ( ( c >> 12 ) & 0x3F ) );
			out[n++] = (char) ( 0x80 | ( ( c >> 6 ) & 0x3F ) );
			out[n++] = (char) ( 0x80 | ( c & 0x3F ) );
		}
	}

	out[n] = '\0';
	return n;
}

/*
 * Only plain words are stemmed. Digits (model numbers like "s23"), wildcards
 * ("tele*"), operators and Manticore's internal marker bytes are left as is.
 */
static int is_stemmable ( const unsigned int * s, int len )
{
	int i;

	for ( i = 0; i < len; i++ )
	{
		unsigned int c = s[i];

		if ( ( c >= 'a' && c <= 'z' ) || c >= 0xC0 )
			continue;

		return 0;
	}

	return 1;
}

int lv_stem_utf8 ( const char * word, char * out, int out_size )
{
	/* one spare slot: unpalatalize() may grow kš -> kst */
	unsigned int cps[LV_STEM_MAX_CODEPOINTS + 1];
	int len, stemmed;

	if ( !word || !out || out_size <= 0 )
		return 0;

	len = utf8_decode ( word, cps, LV_STEM_MAX_CODEPOINTS );
	if ( len <= 0 || !is_stemmable ( cps, len ) )
		return 0;

	stemmed = lv_stem_codepoints ( cps, len );

	if ( utf8_encode ( cps, stemmed, out, out_size ) < 0 )
		return 0;

	return strcmp ( out, word ) != 0;
}
