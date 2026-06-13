#!/bin/sh
# Regenerates the dungeon1 fixture's cfgs, normalizes machine/OS-specific
# tokens, and diffs against committed goldens. Catches project_gen generation
# drift. The OS-tagged link lines are emitted unconditionally, so one golden
# set covers every OS once these four tokens are normalized.
set -e
cd "$(dirname "$0")/../../.."   # repo root
ROOT="$(pwd)"
ENGINE="$ROOT/meikyu"
VENDOR="$ROOT/vendor"
BUILD=build-mac
[ -d "meikyu/$BUILD" ] || BUILD=build-linux
GOLD=meikyu/test/golden
GEN=dungeon1/$BUILD/gen

"./meikyu/$BUILD/meikyu" --path dungeon1 --build game >/dev/null 2>&1 || true

normalize() {
  sed -e "s#$ENGINE#@ENGINE@#g" \
      -e "s#$VENDOR#@VENDOR@#g" \
      -e "s#$ROOT#@ROOT@#g" \
      -e "s#build-mac#@BUILD@#g; s#build-linux#@BUILD@#g" \
      -e "s#^tool glslc .*#tool glslc @GLSLC@#" \
      -e "s#^tool cc .*#tool cc @CC@#" \
      "$1"
}

fail=0
for f in kaji.gen.cfg kansi.gen.cfg game_unity.gen.c; do
  normalize "$GEN/$f" > "/tmp/pg_norm_$f"
  if ! diff "$GOLD/$f" "/tmp/pg_norm_$f"; then
    echo "DRIFT: $f differs from golden" >&2
    fail=1
  fi
done
[ "$fail" = 0 ] && echo "OK: project_gen output matches golden" || exit 1
