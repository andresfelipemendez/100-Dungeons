#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H
#include "arena.h"
#include <toml.h>
enum c_type {
  float_type,
  glmv3_type,
  gluint_type,
  size_t_type,
  submeshp_type,
  path_type,
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

int generate_code_from_buffers(const char *input, char *outputHeader,
                               char *outputSource, size_t size);
size_t generate_struct_data_structure(toml_table_t *conf, Arena *structs_arena,
                                      Arena *strings_arena,
                                      struct_input **structs);
void gen_struct_definitions(struct_input *structs, size_t structs_count,
                            char *output, size_t *offset, size_t size);

size_t serializer_header(struct_input *structs, size_t structs_count,
                         char *output, size_t offset, size_t size);
void serializer_include(struct_input *structs, size_t structs_count,
                        char *output, size_t *offset, size_t size);
void serializer_source(struct_input *structs, size_t structs_count,
                       char *output, size_t *offset, size_t size);

#endif // CODE_GENERATOR_H
