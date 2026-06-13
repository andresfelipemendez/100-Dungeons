#ifndef SENI_ANSWERS_H
#define SENI_ANSWERS_H

/* Pure cores lifted out of host_main's seni reload-answer handling so they
   are unit-testable without a window: the rename-vs-drop annotation choice,
   and the question-mailbox fill clamped at the ABI cap. Depends only on seni
   + the ABI types -- no dodai, no SDL. */

#include "base/base_types.h"
#include "abi/abi_platform.h"
#include "seni.h"   /* arena, annotate_result, diff_question */

/* One answered question -> annotated header. answer is a SENI_ANSWER_* value:
   RENAME -> annotate_rename, DROPPED -> annotate_dropped, anything else ->
   a no-op result (.code == header, .err == NULL). */
annotate_result seni_answer_annotate(arena *a, u32 answer,
                                     const char *struct_name,
                                     const char *removed, const char *added,
                                     char *header);

/* Fill the ABI question mailbox from a diff's questions, clamped at
   SENI_STATUS_MAX_QUESTIONS; each answer set to SENI_ANSWER_NONE. Returns the
   count actually stored (the clamp is the silent-truncation site). */
u32 seni_fill_questions(SeniReloadStatus *status,
                        const diff_question *qs, size_t n);

#endif /* SENI_ANSWERS_H */
