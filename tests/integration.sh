#!/bin/sh
# End-to-end check against a real searchd: loads build/stem_lv.so into the
# official manticoresearch/manticore image, indexes Latvian product names and
# searches them in other grammatical forms.
#
# Usage: tests/integration.sh [image tag]   (default: latest)
set -eu

TAG="${1:-latest}"
NAME="stem-lv-test-$$"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

cleanup() { docker rm -f "$NAME" >/dev/null 2>&1 || true; }
trap cleanup EXIT

docker run -d --name "$NAME" \
  -v "$ROOT/build/stem_lv.so:/usr/share/manticore/modules/stem_lv.so:ro" \
  "manticoresearch/manticore:$TAG" >/dev/null

sql() { docker exec "$NAME" mysql -h0 -P9306 -N -B -e "$1"; }

i=0
until sql "SHOW STATUS" >/dev/null 2>&1; do
  i=$((i + 1))
  [ "$i" -lt 60 ] || { echo "searchd did not start"; docker logs "$NAME"; exit 1; }
  sleep 1
done

PLUGIN_DIR="$(sql "SHOW SETTINGS" | awk -F'\t' '$1 == "common.plugin_dir" { print $2 }')"
if [ "$PLUGIN_DIR" != "/usr/share/manticore/modules" ]; then
  docker exec "$NAME" sh -c "mkdir -p '$PLUGIN_DIR' && cp /usr/share/manticore/modules/stem_lv.so '$PLUGIN_DIR/'"
fi
echo "searchd $(sql "SHOW STATUS LIKE 'version'" | cut -f2), plugin_dir=$PLUGIN_DIR"

sql "CREATE TABLE products (name text) charset_table='non_cjk' min_prefix_len='3' index_token_filter='stem_lv.so:stem_lv:mode=both'"
sql "INSERT INTO products (id, name) VALUES
  (1, 'Televizors Samsung 55 collu'),
  (2, 'Mobilais telefons Apple iPhone'),
  (3, 'Zvaigžņu lukturis bērniem'),
  (4, 'Ledusskapis ar saldētavu')"

FAILED=0

expect() {
  query="$1"
  expected="$2"
  got="$(sql "SELECT id FROM products WHERE MATCH('$query') ORDER BY id ASC OPTION token_filter='stem_lv.so:stem_lv_query:'" | tr '\n' ' ' | sed 's/ $//')"
  if [ "$got" = "$expected" ]; then
    echo "ok   '$query' -> [$got]"
  else
    echo "FAIL '$query' -> [$got], expected [$expected]"
    FAILED=1
  fi
}

expect "televizoru" "1"
expect "televizoriem" "1"
expect "televīzors" "1"
expect "telefoniem" "2"
expect "zvaigzne" "3"
expect "ledusskapji" "4"
expect "saldētava" "4"
expect "samsung" "1"
expect "tele*" "1 2"

# mode=both keeps the original words, so a query without the filter still matches them
got="$(sql "SELECT id FROM products WHERE MATCH('televizors')" | tr '\n' ' ' | sed 's/ $//')"
if [ "$got" = "1" ]; then echo "ok   'televizors' without query filter -> [$got]"; else echo "FAIL original word lookup -> [$got]"; FAILED=1; fi

exit "$FAILED"
