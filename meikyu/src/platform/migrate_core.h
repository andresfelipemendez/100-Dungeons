#ifndef MIGRATE_CORE_H
#define MIGRATE_CORE_H

/* Pure decision logic lifted out of host_main's migrate_hot_memory so the
   data-loss-critical branches are unit-testable without gcc/dlopen. seni's
   e2e suite already covers the actual diff->codegen->compile->run path; this
   covers only the routing and the size guard. */

#include "base/base_types.h"
#include "abi/abi_platform.h"   /* SENI_OVERRIDE_* */
#include "seni.h"               /* diff_result */

typedef enum {
    MIG_NOOP,             /* layouts differ textually, not structurally */
    MIG_PROCEED,          /* clean structural diff: run the migration */
    MIG_REFUSE_QUESTIONS, /* ambiguous, no override: park questions, keep old dll */
    MIG_RELOAD_COLD,      /* ambiguous + cold override: zero hot, load new dll */
    MIG_DROP_ALL          /* ambiguous + drop override: drop ambiguous, migrate rest */
} MigrateAction;

/* Decide what to do with a successful diff (dr->err must be NULL). override is
   the effective SENI_OVERRIDE_* value. */
MigrateAction migrate_decide(const diff_result *dr, u32 override);

/* The compiled migration module reports the new struct size; it must be
   nonzero and fit BOTH the hot and scratch blocks before any copy. */
b32 migrate_size_fits(u64 new_size, u64 hot_size, u64 scratch_size);

#endif /* MIGRATE_CORE_H */
