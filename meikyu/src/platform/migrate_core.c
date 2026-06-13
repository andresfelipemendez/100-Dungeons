#include "migrate_core.h"

MigrateAction migrate_decide(const diff_result *dr, u32 override) {
    if (dr->question_count > 0) {
        if (override == SENI_OVERRIDE_RELOAD_COLD) return MIG_RELOAD_COLD;
        if (override == SENI_OVERRIDE_DROP_ALL)    return MIG_DROP_ALL;
        return MIG_REFUSE_QUESTIONS;
    }
    if (dr->value.struct_count == 0) return MIG_NOOP;
    return MIG_PROCEED;
}

b32 migrate_size_fits(u64 new_size, u64 hot_size, u64 scratch_size) {
    return new_size != 0 && new_size <= hot_size && new_size <= scratch_size;
}
