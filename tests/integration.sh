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
  -v "$ROOT/build/stem_lv.so:/tmp/stem_lv.so:ro" \
  "manticoresearch/manticore:$TAG" >/dev/null

sql() { docker exec "$NAME" mysql -h0 -P9306 -N -B -e "$1"; }

# ids of a SELECT, space separated (the client may draw a table around them)
ids() { sql "$1" | grep -oE '[0-9]+' | tr '\n' ' ' | sed 's/ $//'; }

i=0
until sql "SHOW STATUS" >/dev/null 2>&1; do
  i=$((i + 1))
  [ "$i" -lt 60 ] || { echo "searchd did not start"; docker logs "$NAME"; exit 1; }
  sleep 1
done

PLUGIN_DIR="$(sql "SHOW SETTINGS" | grep plugin_dir | grep -oE '/[^ |	]+' | head -n 1)"
PLUGIN_DIR="${PLUGIN_DIR:-/usr/local/lib/manticore}"
docker exec -u root "$NAME" sh -c "mkdir -p '$PLUGIN_DIR' && cp /tmp/stem_lv.so '$PLUGIN_DIR/'"
echo "plugin_dir: $PLUGIN_DIR"
sql "SHOW STATUS LIKE 'version'"

sql "CREATE TABLE products (name text) charset_table='non_cjk' min_prefix_len='3' index_token_filter='stem_lv.so:stem_lv:mode=both'"
sql "INSERT INTO products (id, name) VALUES
  (1, 'Televizors Samsung 55 collu'),
  (2, 'Mobilais telefons Apple iPhone'),
  (3, 'Zvaigžņu lukturis bērniem'),
  (4, 'Ledusskapis ar saldētavu')"

FAILED=0

check() {
  label="$1"
  got="$2"
  expected="$3"
  if [ "$got" = "$expected" ]; then
    echo "ok   $label -> [$got]"
  else
    echo "FAIL $label -> [$got], expected [$expected]"
    FAILED=1
  fi
}

stemmed() {
  check "'$1'" "$(ids "SELECT id FROM products WHERE MATCH('$1') ORDER BY id ASC OPTION token_filter='stem_lv.so:stem_lv_query:'")" "$2"
}

stemmed "televizoru" "1"
stemmed "televizoriem" "1"
stemmed "televīzors" "1"
stemmed "telefoniem" "2"
stemmed "zvaigzne" "3"
stemmed "ledusskapji" "4"
stemmed "saldētava" "4"
stemmed "samsung" "1"
stemmed "tele*" "1 2"

# mode=both keeps the original words: without the query filter the exact word is found...
check "'televizors' without query filter" "$(ids "SELECT id FROM products WHERE MATCH('televizors')")" "1"
# ...and another form is not, so it is the stemming that matches above
check "'televizoru' without query filter" "$(ids "SELECT id FROM products WHERE MATCH('televizoru')")" ""

exit "$FAILED"
