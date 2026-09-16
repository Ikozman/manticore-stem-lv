/*
 * Unit tests: the Lucene LatvianStemmer test cases, the same cases with
 * diacritics folded (what Manticore's non_cjk charset_table feeds a plugin),
 * and the plugin entry points.
 */

#include <stdio.h>
#include <string.h>

#include "../src/latvian_stemmer.h"

/* plugin entry points from stem_lv.c */
int stem_lv_ver ( void );
int stem_lv_init ( void ** userdata, int num_fields, const char ** field_names, const char * options, char * error_message );
char * stem_lv_push_token ( void * userdata, char * token, int * extra, int * delta );
char * stem_lv_get_extra_token ( void * userdata, int * delta );
void stem_lv_deinit ( void * userdata );
int stem_lv_query_init ( void ** userdata, int max_len, const char * options, char * error_message );
char * stem_lv_query_push_token ( void * userdata, char * token, int * delta, const char * raw_token_start, int raw_token_len );
void stem_lv_query_deinit ( void * userdata );

static int failures = 0;
static int checks = 0;

typedef struct
{
	const char * word;
	const char * stem;
} stem_case;

/* Taken from Lucene's TestLatvianStemmer. */
static const stem_case LUCENE_CASES[] = {
	/* nouns, declension I */
	{ "tēvs", "tēv" }, { "tēvi", "tēv" }, { "tēva", "tēv" }, { "tēvu", "tēv" }, { "tēvam", "tēv" },
	{ "tēviem", "tēv" }, { "tēvus", "tēv" }, { "tēvā", "tēv" }, { "tēvos", "tēv" },
	/* declension II */
	{ "lācis", "lāc" }, { "lāči", "lāc" }, { "lāča", "lāc" }, { "lāču", "lāc" }, { "lācim", "lāc" },
	{ "lāčiem", "lāc" }, { "lāci", "lāc" }, { "lāčus", "lāc" }, { "lācī", "lāc" }, { "lāčos", "lāc" },
	{ "akmens", "akmen" }, { "akmeņi", "akmen" }, { "akmeņu", "akmen" }, { "akmenim", "akmen" },
	{ "akmeņiem", "akmen" }, { "akmeni", "akmen" }, { "akmeņus", "akmen" }, { "akmenī", "akmen" },
	{ "akmeņos", "akmen" },
	{ "kurmis", "kurm" }, { "kurmji", "kurm" }, { "kurmja", "kurm" }, { "kurmju", "kurm" },
	{ "kurmim", "kurm" }, { "kurmjiem", "kurm" }, { "kurmi", "kurm" }, { "kurmjus", "kurm" },
	{ "kurmī", "kurm" }, { "kurmjos", "kurm" },
	/* declension III */
	{ "lietus", "liet" }, { "lieti", "liet" }, { "lietu", "liet" }, { "lietum", "liet" },
	{ "lietiem", "liet" }, { "lietū", "liet" }, { "lietos", "liet" },
	/* declension IV */
	{ "lapa", "lap" }, { "lapas", "lap" }, { "lapu", "lap" }, { "lapai", "lap" }, { "lapām", "lap" },
	{ "lapā", "lap" }, { "lapās", "lap" },
	{ "puika", "puik" }, { "puikas", "puik" }, { "puiku", "puik" }, { "puikam", "puik" },
	{ "puikām", "puik" }, { "puikā", "puik" }, { "puikās", "puik" },
	/* declension V */
	{ "egle", "egl" }, { "egles", "egl" }, { "egļu", "egl" }, { "eglei", "egl" }, { "eglēm", "egl" },
	{ "egli", "egl" }, { "eglē", "egl" }, { "eglēs", "egl" },
	/* declension VI */
	{ "govs", "gov" }, { "govis", "gov" }, { "govju", "gov" }, { "govij", "gov" }, { "govīm", "gov" },
	{ "govi", "gov" }, { "govī", "gov" }, { "govīs", "gov" },
	/* adjectives */
	{ "zils", "zil" }, { "zilais", "zil" }, { "zili", "zil" }, { "zilie", "zil" }, { "zila", "zil" },
	{ "zilā", "zil" }, { "zilas", "zil" }, { "zilās", "zil" }, { "zilu", "zil" }, { "zilo", "zil" },
	{ "zilam", "zil" }, { "zilajam", "zil" }, { "ziliem", "zil" }, { "zilajiem", "zil" },
	{ "zilai", "zil" }, { "zilajai", "zil" }, { "zilām", "zil" }, { "zilajām", "zil" },
	{ "zilus", "zil" }, { "zilos", "zil" }, { "zilajā", "zil" }, { "zilajos", "zil" }, { "zilajās", "zil" },
	/* palatalization */
	{ "krāsns", "krāsn" }, { "krāšņu", "krāsn" }, { "zvaigzne", "zvaigzn" }, { "zvaigžņu", "zvaigzn" },
	{ "kāpslis", "kāpsl" }, { "kāpšļu", "kāpsl" }, { "zizlis", "zizl" }, { "zižļu", "zizl" },
	{ "vilnis", "viln" }, { "viļņu", "viln" }, { "lelle", "lell" }, { "leļļu", "lell" },
	{ "pinne", "pinn" }, { "piņņu", "pinn" }, { "rīkste", "rīkst" }, { "rīkšu", "rīkst" },
	/* length and vowel restrictions */
	{ "usa", "usa" }, { "60ms", "60ms" },
};

/*
 * With diacritics folded the palatalization marks are gone, so these gen. pl.
 * forms can no longer be told apart from a plain stem.
 */
static const char * FOLDED_EXCEPTIONS[] = { "riksu" };

static void fold ( const char * src, char * out, size_t out_size )
{
	static const struct { const char * from; char to; } MAP[] = {
		{ "ā", 'a' }, { "č", 'c' }, { "ē", 'e' }, { "ģ", 'g' }, { "ī", 'i' }, { "ķ", 'k' },
		{ "ļ", 'l' }, { "ņ", 'n' }, { "š", 's' }, { "ū", 'u' }, { "ž", 'z' },
	};
	size_t n = 0;

	while ( *src && n + 1 < out_size )
	{
		size_t i;
		int replaced = 0;

		for ( i = 0; i < sizeof ( MAP ) / sizeof ( MAP[0] ); i++ )
		{
			size_t len = strlen ( MAP[i].from );
			if ( strncmp ( src, MAP[i].from, len ) == 0 )
			{
				out[n++] = MAP[i].to;
				src += len;
				replaced = 1;
				break;
			}
		}

		if ( !replaced )
			out[n++] = *src++;
	}

	out[n] = '\0';
}

static const char * stem_of ( const char * word, char * buffer, size_t size )
{
	if ( !lv_stem_utf8 ( word, buffer, (int) size ) )
		return word;
	return buffer;
}

static void expect_str ( const char * label, const char * input, const char * actual, const char * expected )
{
	checks++;
	if ( ( actual == NULL ) != ( expected == NULL ) || ( actual && strcmp ( actual, expected ) != 0 ) )
	{
		failures++;
		printf ( "FAIL %s: '%s' -> '%s', expected '%s'\n", label, input, actual ? actual : "(null)", expected ? expected : "(null)" );
	}
}

static void expect_int ( const char * label, int actual, int expected )
{
	checks++;
	if ( actual != expected )
	{
		failures++;
		printf ( "FAIL %s: got %d, expected %d\n", label, actual, expected );
	}
}

static void test_lucene_cases ( void )
{
	size_t i;
	char buffer[512];

	for ( i = 0; i < sizeof ( LUCENE_CASES ) / sizeof ( LUCENE_CASES[0] ); i++ )
		expect_str ( "lucene", LUCENE_CASES[i].word, stem_of ( LUCENE_CASES[i].word, buffer, sizeof ( buffer ) ), LUCENE_CASES[i].stem );
}

static void test_folded_cases ( void )
{
	size_t i, j;
	char word[128], expected[128], buffer[512];

	for ( i = 0; i < sizeof ( LUCENE_CASES ) / sizeof ( LUCENE_CASES[0] ); i++ )
	{
		int skip = 0;

		fold ( LUCENE_CASES[i].word, word, sizeof ( word ) );
		fold ( LUCENE_CASES[i].stem, expected, sizeof ( expected ) );

		for ( j = 0; j < sizeof ( FOLDED_EXCEPTIONS ) / sizeof ( FOLDED_EXCEPTIONS[0] ); j++ )
			if ( strcmp ( word, FOLDED_EXCEPTIONS[j] ) == 0 )
				skip = 1;

		if ( !skip )
			expect_str ( "folded", word, stem_of ( word, buffer, sizeof ( buffer ) ), expected );
	}
}

static void test_non_words_are_left_alone ( void )
{
	static const char * WORDS[] = { "s23", "rtx4090", "tele*", "=televizors", "", "a" };
	size_t i;
	char buffer[512];

	for ( i = 0; i < sizeof ( WORDS ) / sizeof ( WORDS[0] ); i++ )
		expect_int ( WORDS[i], lv_stem_utf8 ( WORDS[i], buffer, sizeof ( buffer ) ), 0 );

	expect_int ( "invalid utf-8", lv_stem_utf8 ( "tele\xC3", buffer, sizeof ( buffer ) ), 0 );
	expect_int ( "tiny buffer", lv_stem_utf8 ( "televizoriem", buffer, 4 ), 0 );
}

static void test_product_words ( void )
{
	static const stem_case CASES[] = {
		{ "televizors", "televizor" }, { "televizoru", "televizor" }, { "televizoriem", "televizor" },
		{ "televizori", "televizor" }, { "televizora", "televizor" },
		{ "telefons", "telefon" }, { "telefonu", "telefon" }, { "telefoniem", "telefon" },
		{ "samsung", "samsung" },
	};
	size_t i;
	char buffer[512];

	for ( i = 0; i < sizeof ( CASES ) / sizeof ( CASES[0] ); i++ )
		expect_str ( "product", CASES[i].word, stem_of ( CASES[i].word, buffer, sizeof ( buffer ) ), CASES[i].stem );
}

static void test_index_filter_both ( void )
{
	void * state = NULL;
	char error[256] = "";
	char word[] = "televizoru";
	char digits[] = "s23";
	int extra = -1, delta = 1;
	char * token;

	expect_int ( "init both", stem_lv_init ( &state, 0, NULL, "", error ), 0 );

	token = stem_lv_push_token ( state, word, &extra, &delta );
	expect_str ( "both push", word, token, "televizoru" );
	expect_int ( "both extra", extra, 1 );
	expect_int ( "both delta kept", delta, 1 );

	token = stem_lv_get_extra_token ( state, &delta );
	expect_str ( "both extra token", word, token, "televizor" );
	expect_int ( "both extra delta", delta, 0 );
	expect_str ( "both no more extras", word, stem_lv_get_extra_token ( state, &delta ), NULL );

	token = stem_lv_push_token ( state, digits, &extra, &delta );
	expect_str ( "both digits", digits, token, "s23" );
	expect_int ( "both digits extra", extra, 0 );

	stem_lv_deinit ( state );
}

static void test_index_filter_replace ( void )
{
	void * state = NULL;
	char error[256] = "";
	char word[] = "televizoriem";
	int extra = -1, delta = 1;

	expect_int ( "init replace", stem_lv_init ( &state, 0, NULL, "mode=replace", error ), 0 );
	expect_str ( "replace push", word, stem_lv_push_token ( state, word, &extra, &delta ), "televizor" );
	expect_int ( "replace extra", extra, 0 );
	stem_lv_deinit ( state );

	state = NULL;
	expect_int ( "init bad mode", stem_lv_init ( &state, 0, NULL, "mode=wrong", error ), 1 );
	expect_int ( "bad mode error text", strstr ( error, "unknown mode" ) != NULL, 1 );
}

static void test_query_filter ( void )
{
	void * state = NULL;
	char error[256] = "";
	char word[] = "telefoniem";
	char wildcard[] = "tele*";
	int delta = 0;

	expect_int ( "query init", stem_lv_query_init ( &state, 128, "", error ), 0 );
	expect_str ( "query push", word, stem_lv_query_push_token ( state, word, &delta, word, 10 ), "telefon" );
	expect_str ( "query wildcard", wildcard, stem_lv_query_push_token ( state, wildcard, &delta, wildcard, 5 ), "tele*" );
	expect_int ( "query delta", delta, 0 );
	stem_lv_query_deinit ( state );
}

int main ( void )
{
	expect_int ( "version", stem_lv_ver (), 12 );

	test_lucene_cases ();
	test_folded_cases ();
	test_non_words_are_left_alone ();
	test_product_words ();
	test_index_filter_both ();
	test_index_filter_replace ();
	test_query_filter ();

	printf ( "%d checks, %d failures\n", checks, failures );
	return failures ? 1 : 0;
}
