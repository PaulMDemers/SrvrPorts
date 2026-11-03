#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/../../.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/srvros-awk.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
python3 "$root/ports/srvros/awk/prepare.py" "$root/ports/upstream/awk" "$tmp/src"
python3 "$root/ports/srvros/awk/prepare.py" "$root/ports/upstream/awk" "$tmp/src2"
# The manifest contains hashes of all generated text and source inputs.
cmp "$tmp/src/manifest.json" "$tmp/src2/manifest.json"
git clone -q --no-hardlinks "$root/ports/upstream/awk" "$tmp/modified"
printf '\n/* deliberate test change */\n' >> "$tmp/modified/main.c"
if python3 "$root/ports/srvros/awk/prepare.py" "$tmp/modified" "$tmp/rejected" >"$tmp/rejected.log" 2>&1; then
  printf '%s\n' 'modified input was accepted' >&2
  exit 1
fi
[ ! -e "$tmp/rejected/.prepared" ]
# HOSTCC may contain compiler options, just as in prepare.py.
# shellcheck disable=SC2086
${HOSTCC:-cc} -include "$root/ports/srvros/awk/config.h" -I "$tmp/src" \
  "$tmp/src/awkgram.tab.c" "$tmp/src/b.c" "$tmp/src/main.c" \
  "$tmp/src/parse.c" "$tmp/src/proctab.c" "$tmp/src/tran.c" \
  "$tmp/src/lib.c" "$tmp/src/run.c" "$tmp/src/lex.c" \
  "$root/ports/srvros/awk/compat.c" -lm -o "$tmp/awk"
result=$(printf 'alice 3\nbob 7\n' | "$tmp/awk" '{s += $2} END {print NR, s, toupper("Abc"), tolower("XYZ")}')
[ "$result" = '2 10 ABC xyz' ]
"$tmp/awk" 'BEGIN {srand(42); a=rand(); b=rand(); srand(42); if (a != rand() || b != rand() || a < 0 || a >= 1 || b < 0 || b >= 1 || a == b) exit 1}'
printf '%s\n' 'awk host smoke passed'
