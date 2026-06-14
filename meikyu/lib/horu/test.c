/* horu unit tests. Built + run via `meikyu --test horu` (see
   docs/superpowers/specs/2026-06-13-kaji-lib-test-builds-design.md).
   Strict C89: declarations at block top, no // comments. */

#include "horu.h"
#include <stdio.h>
#include <math.h>

static int g_fail = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        g_fail++; \
    } \
} while (0)

static int feq(float a, float b) { return (float)fabs(a - b) < 1e-5f; }

/* ---- plane construction -------------------------------------------------- */

static void test_plane_make_normalizes(void) {
    /* input normal (0,3,0) with d=6 describes the set 3y = 6, i.e. y = 2.
       horu_plane_make must return a UNIT normal and scale d to match, so the
       described plane is unchanged: normal (0,1,0), d = 2. */
    horu_plane p = horu_plane_make(0.0f, 3.0f, 0.0f, 6.0f);
    CHECK(feq(p.nx, 0.0f));
    CHECK(feq(p.ny, 1.0f));
    CHECK(feq(p.nz, 0.0f));
    CHECK(feq(p.d, 2.0f));
}

int main(void) {
    test_plane_make_normalizes();
    if (g_fail) {
        printf("%d check(s) failed\n", g_fail);
        return 1;
    }
    printf("horu geometry: all checks passed\n");
    return 0;
}
