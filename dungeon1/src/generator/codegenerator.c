#include "codegenerator.h"
#include "arena.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <toml.h>

static void error(const char *msg, const char *msg1) {
  fprintf(stderr, "ERROR: %s%s\n", msg, msg1 ? msg1 : "");
  exit(1);
}

int generate_code_from_buffers(const char *input, char *outputHeader,
                               char *outputSource, size_t size) {
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

  size_t header_offset = 0;
  gen_struct_definitions(structs, structs_count, outputHeader, &header_offset,
                         size);

  size_t source_offset = 0;
  serializer_include(structs, structs_count, outputSource, &source_offset,
                     size);
  serializer_source(structs, structs_count, outputSource, &source_offset, size);

  arena_destroy(structs_arena);
  arena_destroy(strings_arena);
  return 1;
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
    current_struct->fields =
        (field *)arena_alloc(structs_arena, sizeof(field) * field_count);
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
      curren_field->name[field_name_len - 1] = '\0';
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

#define APPEND(fmt, ...)                                                       \
  *o += snprintf(output + *o, size - *o, fmt, __VA_ARGS__)

void gen_struct_definitions(struct_input *structs, size_t structs_count,
                            char *output, size_t *o, size_t size) {
  for (size_t i = 0; i < structs_count; i++) {
    APPEND("#define %sType\n", structs[i].name);
  }
  APPEND("\n");
  APPEND("enum ComponentBitmask {\n");
  for (size_t i = 0; i < structs_count; i++) {
    APPEND("\t%sComponent = (1 << %zu),\n", structs[i].name, i);
  }
  APPEND("};\n");
  APPEND("\n");
  for (size_t i = 0; i < structs_count; i++) {
    APPEND("struct %s {\n", structs[i].name);
    for (size_t j = 0; j < structs[i].field_count; j++) {
      char *field_type_name = NULL;
      switch (structs[i].fields[j].type) {
      case float_type:
        field_type_name = "float";
      }
      if (field_type_name == NULL) {
        error("type not found", NULL);
      }
      APPEND("\t%s %s;\n", field_type_name, structs[i].fields[j].name);
    }
    APPEND("};\n\n");
  }

  for (size_t i = 0; i < structs_count; i++) {
    APPEND("bool add_component(struct MemoryHeader *h, size_t entity_id, %s "
           "component);\n",
           structs[i].name);
  }
  APPEND("\n");
  for (size_t i = 0; i < structs_count; i++) {
    APPEND("bool get_component(struct MemoryHeader *h, size_t entity_id, %s "
           "*component);\n",
           structs[i].name);
  }
  APPEND("\n");
  for (size_t i = 0; i < structs_count; i++) {
    APPEND("bool set_component(struct MemoryHeader *h, size_t entity_id, %s "
           "*component);\n",
           structs[i].name);
  }
}

void serializer_include(struct_input *, size_t, char *output, size_t *o,
                        size_t size) {
  APPEND("#include \"components.gen.h\"\n"
         "#include \"ecs.h\"\n\n");
}

void serializer_source(struct_input *structs, size_t structs_count,
                       char *output, size_t *o, size_t size) {

  for (size_t i = 0; i < structs_count; i++) {
    // APPEND(
  }

  APPEND(
      "void save_level(MemoryHeader *h, const char *saveFilePath) {\n"
      "\tFILE *fp = fopen(saveFilePath, \"w\");\n"
      "\tif (!fp) {\n"
      "\t\tprintf(\"Failed to open file %%s for writing.\\n\", saveFilePath);\n"
      "\t\treturn;\n"
      "\t}\n"
      "\n"
      "\tWorld *w = &h->world;\n"
      "\tfor (size_t i = 0; i < w->entity_count; ++i) {\n"
      "\tsize_t entity_id = w->entity_ids[i];\n"
      "\tuint32_t mask = w->component_masks[entity_id];\n");

  for (size_t i = 0; i < structs_count; i++) {
    APPEND("\tif(mask & %sComponent) {\n", structs[i].name);
    size_t name_len = strlen(structs[i].name) + 1;
    char lowerName[name_len];
    strcpy_s(lowerName, name_len, structs[i].name);
    lowerName[0] = tolower(lowerName[0]);
    APPEND("\t%s %s;\n", structs[i].name, lowerName);
    APPEND("\tif (get_component(h, entity_id, &%s)) {\n", lowerName);
    APPEND("\t\tfprintf(fp,\"%s = { ", lowerName);
    for (size_t j = 0; j < structs[i].field_count; j++) {
      char *separator = (j == structs[i].field_count - 1) ? "" : ",";
      char *type_serializer;
      switch (structs[i].fields[j].type) {
      case float_type:
        type_serializer = "%.2f";
        break;
      default:
        type_serializer = "";
        error("Unsupported field type: ", structs[i].fields[j].name);
        break;
      }
      APPEND("%s = %s%s ", structs[i].fields[j].name, type_serializer,
             separator);
    }
    APPEND("}\",");
    for (size_t j = 0; j < structs[i].field_count; j++) {
      char *separator = (j == structs[i].field_count - 1) ? "" : ",";

      APPEND(" %s.%s%s", lowerName, structs[i].fields[j].name, separator);
    }
    APPEND(");\n\t}\n}\n");
  }

  APPEND("fprintf(fp, \"\\n\");\n"
         "}\n"
         "fclose(fp);\n"
         "printf(\"World saved to %%s\\n\", saveFilePath);\n"
         "}\n");
}
