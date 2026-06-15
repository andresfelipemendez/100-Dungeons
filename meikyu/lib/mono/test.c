/* mono core tests: sparse generational entity table, headless. Strict C89. */

#include "mono.h"
#include <stdio.h>
#include <string.h>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static mono_world g_w;

static void test_init(void) {
    int i, live = 0;
    mono_init(&g_w);
    CHECK(g_w.count == 0);
    for (i = 0; i < MONO_MAX; i++) if (g_w.ents[i].kind != MONO_NONE) live++;
    CHECK(live == 0);
    CHECK(mono_get(&g_w, MONO_INVALID) == NULL);
    CHECK(!mono_alive(&g_w, MONO_INVALID));
}

static void test_spawn_get(void) {
    mono_id id;
    Entity *e;
    mono_init(&g_w);
    id = mono_spawn(&g_w, MONO_PROP);
    CHECK(id != MONO_INVALID);
    CHECK(g_w.count == 1);
    CHECK(mono_alive(&g_w, id));
    e = mono_get(&g_w, id);
    CHECK(e != NULL);
    CHECK(e->kind == MONO_PROP);
    /* identity transform on spawn */
    CHECK(e->px == 0.0f && e->py == 0.0f && e->pz == 0.0f);
    CHECK(e->rx == 0.0f && e->ry == 0.0f && e->rz == 0.0f);
    CHECK(e->sx == 1.0f && e->sy == 1.0f && e->sz == 1.0f);
    /* MONO_NONE is the free marker, not a spawnable kind */
    CHECK(mono_spawn(&g_w, MONO_NONE) == MONO_INVALID);
    CHECK(g_w.count == 1);
}

static void test_destroy(void) {
    mono_id id;
    mono_init(&g_w);
    id = mono_spawn(&g_w, MONO_PLAYER);
    mono_destroy(&g_w, id);
    CHECK(g_w.count == 0);
    CHECK(!mono_alive(&g_w, id));
    CHECK(mono_get(&g_w, id) == NULL);
    /* destroy is idempotent: a second destroy (now stale) is a no-op */
    mono_destroy(&g_w, id);
    CHECK(g_w.count == 0);
}

static void test_generation_stale(void) {
    mono_id a, b;
    mono_init(&g_w);
    a = mono_spawn(&g_w, MONO_PROP);  /* takes slot 0 */
    mono_destroy(&g_w, a);
    b = mono_spawn(&g_w, MONO_LIGHT); /* reuses slot 0, bumped generation */
    CHECK(b != MONO_INVALID);
    CHECK(b != a);                    /* same index, different generation */
    CHECK((a & 0xFFFFu) == (b & 0xFFFFu)); /* ...indeed the same slot */
    CHECK(mono_get(&g_w, a) == NULL); /* the old handle is stale */
    CHECK(mono_alive(&g_w, b));
    CHECK(mono_get(&g_w, b)->kind == MONO_LIGHT);
}

static void test_spawn_clean_slate(void) {
    /* a reused slot must not leak the previous entity's fields (the fat-struct
       hazard): spawn clears everything but the generation. */
    mono_id a, b;
    Entity *e;
    mono_init(&g_w);
    a = mono_spawn(&g_w, MONO_PROP);
    e = mono_get(&g_w, a);
    e->px = 9.0f; e->py = 9.0f; e->pz = 9.0f;  /* dirty the slot */
    e->ry = 45.0f;
    mono_destroy(&g_w, a);
    b = mono_spawn(&g_w, MONO_PROP);            /* reuses slot 0 */
    e = mono_get(&g_w, b);
    CHECK(e->px == 0.0f && e->py == 0.0f && e->pz == 0.0f); /* no stale data */
    CHECK(e->ry == 0.0f);
    CHECK(e->sx == 1.0f && e->sy == 1.0f && e->sz == 1.0f); /* identity scale */
}

static void test_full(void) {
    int i;
    mono_init(&g_w);
    for (i = 0; i < MONO_MAX; i++) {
        CHECK(mono_spawn(&g_w, MONO_PROP) != MONO_INVALID);
    }
    CHECK(g_w.count == MONO_MAX);
    CHECK(mono_spawn(&g_w, MONO_PROP) == MONO_INVALID); /* table full */
    CHECK(g_w.count == MONO_MAX);
}

static void test_bad_handles(void) {
    mono_id id;
    mono_init(&g_w);
    id = mono_spawn(&g_w, MONO_PROP);
    /* an out-of-range index in a handle resolves to NULL, not a read past end */
    CHECK(mono_get(&g_w, MONO_ID(MONO_MAX + 5, 1)) == NULL);
    /* a right slot but wrong generation is stale */
    CHECK(mono_get(&g_w, MONO_ID(id & 0xFFFFu, 999)) == NULL);
    /* destroying garbage is a no-op, live entity untouched */
    mono_destroy(&g_w, MONO_ID(MONO_MAX + 5, 1));
    CHECK(mono_alive(&g_w, id));
    CHECK(g_w.count == 1);
}

static void test_iterate(void) {
    mono_id ids[3];
    int i, seen = 0;
    mono_init(&g_w);
    for (i = 0; i < 3; i++) ids[i] = mono_spawn(&g_w, MONO_PROP);
    mono_destroy(&g_w, ids[1]);  /* leave a hole */
    for (i = 0; i < MONO_MAX; i++) if (g_w.ents[i].kind != MONO_NONE) seen++;
    CHECK(seen == 2);
    CHECK(g_w.count == 2);
}

int main(void) {
    test_init();
    test_spawn_get();
    test_destroy();
    test_generation_stale();
    test_spawn_clean_slate();
    test_full();
    test_bad_handles();
    test_iterate();
    if (g_fail) { printf("%d check(s) failed\n", g_fail); return 1; }
    printf("mono: all checks passed\n");
    return 0;
}
