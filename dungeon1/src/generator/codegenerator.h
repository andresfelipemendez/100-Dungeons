#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H

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

int generate_code_from_buffers(const char *input, char *output, size_t size);

#endif // CODE_GENERATOR_H
