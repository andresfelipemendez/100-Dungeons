#!/bin/sh
# Fails if cmake's compiled host sources diverge from build.manifest host_src
# (plus the per-OS dodai source). This is the check that catches "added a .c to
# cmake but not the manifest" -- the exact drift this whole mechanism prevents.
set -e
cd "$(dirname "$0")/../../.."   # repo root
ENGINE=meikyu
BUILD="$ENGINE/build-mac"
[ -d "$BUILD" ] || BUILD="$ENGINE/build-linux"
[ -d "$BUILD" ] || { echo "no build dir; bootstrap first" >&2; exit 1; }

# manifest's view (host_src + the OS dodai source), sorted.
"$BUILD/meikyu" --print-host-srcs | sort > /tmp/drift_manifest.txt

# cmake's view: the .c sources ninja compiles for target 'meikyu', made
# relative to the engine dir, vendored SDL (_deps) dropped, sorted.
ninja -C "$BUILD" -t inputs meikyu 2>/dev/null \
  | grep -E '\.c$' \
  | sed -E "s#.*/$ENGINE/##; s#^\.\./##" \
  | grep -vE '_deps/' \
  | sort -u > /tmp/drift_cmake.txt

echo "=== manifest host_src (+dodai) ==="; cat /tmp/drift_manifest.txt
echo "=== cmake compiled .c ===";          cat /tmp/drift_cmake.txt
if ! diff /tmp/drift_manifest.txt /tmp/drift_cmake.txt; then
  echo "DRIFT: cmake host sources != build.manifest host_src" >&2
  exit 1
fi
echo "OK: cmake and build.manifest agree on host sources"
