#include "codegenerator.h"
#include "arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <toml.h>

static void error(const char *msg, const char *msg1) {
  fprintf(stderr, "ERROR: %s%s\n", msg, msg1 ? msg1 : "");
  exit(1);
}

int open_struct(char *output, size_t size, int offset, const char *structName) {
  return snprintf(output + offset, size - offset, "struct %s {\n", structName);
}
int close_struct(char *output, size_t size, int offset) {
  return snprintf(output + offset, size - offset, "};\n");
}
int struct_field(char *output, size_t size, int offset, char *type,
                 const char *name) {
  return snprintf(output + offset, size - offset, "\t%s %s;\n", type, name);
}

enum c_type {
  float_type,
};

typedef struct {
  char *name;
  enum c_type type;
} field;
typedef struct struct_input {
  char *struct_name;
} struct_input;

int generate_code_from_buffers(const char *input, char *output, size_t size) {
  char errbuf[200];
  toml_table_t *conf = toml_parse((char *)input, errbuf, sizeof(errbuf));

  if (!conf) {
    error("cannot parse - ", errbuf);
  }
  size_t o = 0;

  struct_input *structs;
  for (int i = 0;; i++) {
    const char *structName = toml_key_in(conf, i);
    if (!structName)
      break;

    o += open_struct(output, size, o, structName);
    toml_table_t *fields = toml_table_in(conf, structName);
    if (!fields)
      break;
    for (int j = 0;; j++) {
      const char *field_name = toml_key_in(fields, j);
      if (!field_name)
        break;

      toml_datum_t field_type = toml_string_in(fields, field_name);
      if (field_type.ok) {
        o += struct_field(output, size, o, field_type.u.s, field_name);
        free(field_type.u.s);
      }
    }
    o += close_struct(output, size, o);
  }

  return 0;
}
