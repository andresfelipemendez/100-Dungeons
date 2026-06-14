#ifndef TESTS_GEN_H
#define TESTS_GEN_H

/* tests_gen: turns the build.manifest lib_test table into a kaji cfg the
   engine feeds to kaji in --test mode. Sibling to project_gen, but a PURE
   emitter (no globals, no filesystem) so it is unit-testable like
   build_manifest -- a thin runtime writer wraps it where --test lives.

   One `exe` target per lib_test row (named test_<lib>), built through kaji
   with the row's std/pedantic/src/include/link. `test.c` is implicit; a
   dodai_*.c src is rewritten to the active platform's dodai source.

   With coverage set, every target gets the `coverage` key and its output
   lands under gen/cov/ (instrumented binaries differ from plain ones).

   See docs/superpowers/specs/2026-06-13-kaji-lib-test-builds-design.md */

#include "base/base_types.h"
#include "../../lib/ito/ito.h"
#include "build_manifest.h"

/* Emit the cfg text into `out` (a caller-sized ito_buf). engine_root is the
   absolute path to meikyu/. cc is the resolved compiler, written as the cfg's
   `tool cc` (must be clang when coverage is set -- the MC/DC backend). */
void tests_gen_emit(ito_buf *out, const BuildManifest *m, const BuildPlatform *p,
                    const char *engine_root, const char *cc, b32 coverage);

#endif /* TESTS_GEN_H */
