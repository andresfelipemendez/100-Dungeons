#include "codegenerator.h"
#include "arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <toml.h>

static void error(const char *msg, const char *msg1) {
  fprintf(stderr, "ERROR: %s%s\n", msg, msg1 ? msg1 : "");
  exit(1);
}

int generate_code_from_buffers(const char *input, char *output, size_t size) {
  char errbuf[200];
  toml_table_t *conf = toml_parse((char *)input, errbuf, sizeof(errbuf));
  if (!conf) {
    error("cannot parse - ", errbuf);
  }
  Arena *structs_arena = arena_create(1024);
  Arena *strings_arena = arena_create(1024);
  struct_input *structs = NULL;
  size_t structs_count = generate_struct_data_structure(
      conf, structs_arena, strings_arena, &structs);

  size_t o = generate_struct_definitions(structs, structs_count, output, size);
  arena_destroy(structs_arena);
  arena_destroy(strings_arena);
  return 0;
}

size_t generate_struct_data_structure(toml_table_t *conf, Arena *structs_arena,
                                      Arena *strings_arena,
                                      struct_input **structs) {
  // #define print_debug

  size_t structs_count = 0;
  for (int i = 0;; i++) {
    const char *structName = toml_key_in(conf, i);
    if (!structName)
      break;
    structs_count++;
  }
  *structs = (struct_input *)arena_alloc(structs_arena,
                                         sizeof(struct_input) * structs_count);
  structs_count = 0;
  for (int i = 0;; i++) {
    const char *structName = toml_key_in(conf, i);
    if (!structName)
      break;
    toml_table_t *fields = toml_table_in(conf, structName);
    if (!fields)
      break;

    size_t struct_name_len = strlen(structName) + 1;
    struct_input *current_struct = &(*structs)[structs_count];
    current_struct->name = (char *)arena_alloc(strings_arena, struct_name_len);
    if (!current_struct->name) {
      error("Couldn't allocate memory for struct name", structName);
    }
    strcpy_s(current_struct->name, struct_name_len, structName);
    current_struct->name[struct_name_len - 1] = '\0';
#ifdef print_debug
    printf("copyed struct name: %s\n", current_struct->name);
#endif

    size_t field_count = 0;
    for (int j = 0;; j++) {
      const char *field_name = toml_key_in(fields, j);
      if (!field_name)
        break;
      field_count++;
    }
#ifdef print_debug
    printf("allocating %zu fields\n", field_count);
#endif
    current_struct->fields = (field *)arena_alloc(structs_arena, field_count);
    current_struct->field_count = 0;
    for (int j = 0;; j++) {
      const char *field_name = toml_key_in(fields, j);
      if (!field_name)
        break;
      toml_datum_t field_type_name = toml_string_in(fields, field_name);
      if (!field_type_name.ok)
        break;
      field *curren_field =
          &current_struct->fields[current_struct->field_count];
      size_t field_name_len = strlen(field_name) + 1;

      curren_field->name = (char *)arena_alloc(strings_arena, field_name_len);
      strcpy_s(curren_field->name, field_name_len, field_name);
      curren_field->name[struct_name_len - 1] = '\0';
#ifdef print_debug
      printf("copyed field name: %s\n", curren_field->name);
#endif
      if (strcmp(field_type_name.u.s, "float") == 0) {
        current_struct->fields[current_struct->field_count].type = float_type;
      }
      current_struct->field_count++;
      free(field_type_name.u.s);
    }

    structs_count++;
  }
#ifdef print_debug
  printf("structs count %zu\n", structs_count);
#endif
#undef print_debug
  return structs_count;
}

size_t generate_struct_definitions(struct_input *structs, size_t structs_count,
                                   char *output, size_t size) {

  size_t o = 0;
  for (size_t i = 0; i < structs_count; i++) {
    o += snprintf(output + o, size - o, "struct %s {\n", structs[i].name);
    // printf("parsed struct: %s\n", structs[i].name);
    for (size_t j = 0; j < structs[i].field_count; j++) {
      char *field_type_name = NULL;
      switch (structs[i].fields[j].type) {
      case float_type:
        field_type_name = "float";
      }
      if (field_type_name == NULL) {
        error("type not found", NULL);
      }
      // printf("%s %s\n", structs[i].fields[j].name, field_type_name);
      o += snprintf(output + o, size - o, "\t%s %s;\n", field_type_name,
                    structs[i].fields[j].name);
    }
    o += snprintf(output + o, size - o, "};\n");
  }
  return o;
}
