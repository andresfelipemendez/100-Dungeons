#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H
#include "arena.h"
#include <toml.h>
enum c_type {
  float_type,
};

typedef struct field {
  char *name;
  enum c_type type;
} field;

typedef struct struct_input {
  char *name;
  field *fields;
  size_t field_count;
} struct_input;

// typedef struct structs
// {
// 	struct_input* array_structs;

// } structs;

int generate_code_from_buffers(const char *input, char *output, size_t size);
size_t generate_struct_data_structure(toml_table_t *conf, Arena *structs_arena,
                                      Arena *strings_arena,
                                      struct_input **structs);
size_t gen_struct_definitions(struct_input *structs, size_t structs_count,
                              char *output, size_t offset, size_t size);

size_t gen_struct_serializer(struct_input *structs, size_t structs_count,
                             char *output, size_t offset, size_t size);

#endif // CODE_GENERATOR_H
