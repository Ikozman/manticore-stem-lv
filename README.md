# manticore-stem-lv

Latvian stemming for [Manticore Search](https://manticoresearch.com/).

Manticore has no Latvian stemmer (`stem_lv` / `libstemmer_lv` do not exist), so a search for
`televizoru` does not find `Televizors`. This project adds one as a pair of token filter plugins.
The algorithm is a C port of Apache Lucene's
[`LatvianStemmer`](https://lucene.apache.org/core/9_12_0/analysis/common/org/apache/lucene/analysis/lv/LatvianStemmer.html)
(the one Elasticsearch and OpenSearch use): a light stemmer for noun and adjective endings,
with palatalization handling (`lāči → lāc`, `zvaigžņu → zvaigzn`).

```
televizors, televizoru, televizoriem, televizora  →  televizor
telefons, telefonu, telefoniem                    →  telefon
zils, zilais, zilajiem, zilām                     →  zil
```

## Plugins

One library, `stem_lv.so` (Linux) or `stem_lv.dll` (Windows), with two filters:

| Filter | Type | What it does |
|---|---|---|
| `stem_lv` | index-time | adds the stem of every word to the index |
| `stem_lv_query` | query-time | replaces every query word with its stem |

Both are needed: the index filter stores stems, the query filter stems what the user types.

Words with digits (`s23`, `rtx4090`), wildcards (`tele*`) and operators are left as they are.
Words work both with and without diacritics, so `charset_table = 'non_cjk'`, which folds
`ī → i`, is fine.

## Install

1. Download `stem_lv-linux-x86_64.so` or `stem_lv-windows-x64.dll` from
   [Releases](../../releases), or build it (see below).
2. Put it into searchd's `plugin_dir` as `stem_lv.so` / `stem_lv.dll`.
   The current value is shown by `SHOW SETTINGS` (`common.plugin_dir`);
   set it in the `common` section of `manticore.conf` if needed:
   ```ini
   common {
       plugin_dir = /usr/local/lib/manticore
   }
   ```
3. Restart searchd if you changed the config.

In Docker, mount the file into the plugin directory:

```sh
docker run -v $PWD/stem_lv.so:/usr/local/lib/manticore/stem_lv.so manticoresearch/manticore
```

## Use

Create the table with the index filter:

```sql
CREATE TABLE products (name text)
  charset_table = 'non_cjk'
  min_prefix_len = '3'
  index_token_filter = 'stem_lv.so:stem_lv:mode=both';
```

Search with the query filter:

```sql
SELECT id, name FROM products
WHERE MATCH('televizoriem')
OPTION token_filter = 'stem_lv.so:stem_lv_query:';
```

Use `stem_lv.dll` in both specs on Windows.

`index_token_filter` is fixed when the table is created. To add it to an existing table,
recreate the table and reindex.

### Index modes

| Option | Indexed for `televizoru` | Notes |
|---|---|---|
| `mode=both` (default) | `televizoru` and `televizor` at the same position | queries without the query filter still find the exact word |
| `mode=replace` | `televizor` | smaller index; always search with the query filter |

## Limits

- A light stemmer: nouns and adjectives only, verbs are not stemmed.
- Ambiguous palatalizations (`s/t → š`, `d/z → ž`) are not reversed, as in Lucene.
  With diacritics folded, `rīkšu` no longer maps to `rīkst`.
- It stems whatever it gets, so English or brand words are stemmed too (`iphone → iphon`).
  That is harmless as long as the index and the query use the same filters.
- Use it on a Latvian table, not together with `morphology` for other languages.

## Build

Requires a C99 compiler and `make`.

```sh
make test      # unit tests: Lucene's LatvianStemmer cases, plain and with diacritics folded
make linux     # build/stem_lv.so
make windows   # build/stem_lv.dll, needs x86_64-w64-mingw32-gcc
sh tests/integration.sh   # loads build/stem_lv.so into manticoresearch/manticore in Docker
```

GitHub Actions runs all of it on every push; a `v*` tag publishes both libraries to Releases.

The plugin ABI version is `12` (`SPH_UDF_VERSION` in Manticore's `src/sphinxudf.h`).
searchd loads a library built for the same or a newer version.

## License

Apache License 2.0, see [LICENSE](LICENSE) and [NOTICE](NOTICE).
The stemmer is derived from Apache Lucene.
