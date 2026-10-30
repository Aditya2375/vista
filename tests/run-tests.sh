#!/bin/sh
# vista test suite: widget/layout/event unit checks and a golden dump of a
# scripted form session (type, toggle, navigate, save).
set -e
cd "$(dirname "$0")/.."
OUT=build/tests
mkdir -p "$OUT"

TCC="${CC:-cc}"
TCFLAGS="-std=gnu11 -O2 -g -Wall -Wextra -Werror -Isrc"

$TCC $TCFLAGS tests/test_vista.c build/libvista.a -o "$OUT/test_vista"
"$OUT/test_vista"

$TCC $TCFLAGS tests/test_round2.c build/libvista.a -o "$OUT/test_round2"
"$OUT/test_round2"

$TCC $TCFLAGS -Iexamples -DDEMO_NO_MAIN \
    tests/dump_demo.c examples/demo.c build/libvista.a -o "$OUT/dump_demo"
"$OUT/dump_demo" > "$OUT/demo.txt"
diff tests/demo.expected "$OUT/demo.txt"
echo "GOLDEN DUMP PASSED"
echo "ALL TESTS PASSED"
