#ifndef MUTATE_H
#define MUTATE_H

/* mutate: a first-party mutation-testing scanner. Given C source text, it
   enumerates MUTATION SITES -- single-operator swaps that should change the
   program's meaning (a relational or logical operator flipped). The engine's
   --mutate mode applies each in turn (backup -> patch -> kaji rebuild ->
   meikyu --test -> killed iff a test fails -> restore) and reports the
   mutation score; survivors are real gaps the tests do not pin down.

   Pure + unit-testable (no filesystem): string and char literals and both
   comment styles are skipped so an operator inside them is never mutated.

   v1 operators (each occurrence is one mutant):
       <= <-> <      >= <-> >      == <-> !=      && <-> ||
   Bit shifts (<< >>), ->, and arithmetic are intentionally left out: they are
   ambiguous against unary/pointer/template-free C and would yield noise.

   See docs/superpowers/specs/2026-06-13-kaji-lib-test-builds-design.md */

#define MUTATE_MAX 4096   /* cap on sites per file */

typedef struct {
    int  offset;     /* byte offset of the operator in the source */
    int  len;        /* length of the original operator (1 or 2) */
    char repl[4];    /* NUL-terminated replacement operator */
    int  line;       /* 1-based line, for reporting */
} mutant;

/* Scan `src` (len bytes); write up to `max` sites into out[]. Returns the
   count (clamped to max). */
int mutate_scan(const char *src, int len, mutant *out, int max);

#endif /* MUTATE_H */
