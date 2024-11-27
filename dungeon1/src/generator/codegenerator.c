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
      if (strcmp(field_type_name.u.s, "glm::vec3") == 0) {
        current_struct->fields[current_struct->field_count].type = glmv3_type;
      }
      if (strcmp(field_type_name.u.s, "GLuint") == 0) {
        current_struct->fields[current_struct->field_count].type = gluint_type;
      }
      if (strcmp(field_type_name.u.s, "size_t") == 0) {
        current_struct->fields[current_struct->field_count].type = size_t_type;
      }
      if (strcmp(field_type_name.u.s, "path") == 0) {
        current_struct->fields[current_struct->field_count].type = path_type;
      }

      if (strcmp(field_type_name.u.s, "SubMesh*") == 0) {
        current_struct->fields[current_struct->field_count].type =
            submeshp_type;
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
  APPEND("#include \"ecs.h\"\n"
         "#include <glm.hpp>\n"
         "#include <glad.h>\n");

  APPEND("enum ComponentType {\n");
  for (size_t i = 0; i < structs_count; i++) {
    APPEND("\t%sType,\n", structs[i].name);
  }
  APPEND("\tUNKNOWN_TYPE\n"
         "};\n\n");

  APPEND("enum ComponentBitmask {\n");
  for (size_t i = 0; i < structs_count; i++) {
    APPEND("\t%sComponent = (1 << %zu),\n", structs[i].name, i);
  }
  APPEND("};\n");
  APPEND("\n");
  APPEND("extern const char *component_names[];\n"
         "extern size_t component_count;\n");
  APPEND("\n");
  for (size_t i = 0; i < structs_count; i++) {
    APPEND("struct %s {\n", structs[i].name);
    for (size_t j = 0; j < structs[i].field_count; j++) {
      char *field_type_name = NULL;
      switch (structs[i].fields[j].type) {
      case float_type: {
        field_type_name = "float";
        break;
      }
      case glmv3_type: {
        field_type_name = "glm::vec3";
        break;
      }
      case gluint_type: {
        field_type_name = "GLuint";
        break;
      }
      case size_t_type: {
        field_type_name = "size_t";
        break;
      }
      case submeshp_type: {
        field_type_name = "SubMesh*";
        break;
      }
      case path_type: {
        field_type_name = "char*";
        break;
      }
      }
      if (field_type_name == NULL) {
        error("type not found", NULL);
      }
      APPEND("\t%s %s;\n", field_type_name, structs[i].fields[j].name);
    }
    APPEND("};\n\n");
  }

  for (size_t i = 0; i < structs_count; i++) {
    APPEND("typedef struct %ss {\n"
           "\tsize_t count;\n"
           "\tsize_t *entity_ids;\n"
           "\t%s *components;\n"
           "} %ss;\n\n",
           structs[i].name, structs[i].name, structs[i].name);
  }

  APPEND("struct Components {\n");
  for (size_t i = 0; i < structs_count; i++) {
    APPEND("\t%ss *p%ss;\n", structs[i].name, structs[i].name);
  }
  APPEND("};\n\n");

  APPEND("ComponentType mapStringToComponentType(const char * type_key);\n");

  for (size_t i = 0; i < structs_count; i++) {
    APPEND("void add_component(Memory *m, size_t entity_id, %s "
           "component);\n",
           structs[i].name);
  }
  APPEND("\n");
  for (size_t i = 0; i < structs_count; i++) {
    APPEND("bool get_component(Memory *m, size_t entity_id, %s "
           "*component);\n",
           structs[i].name);
  }
  APPEND("\n");
  for (size_t i = 0; i < structs_count; i++) {
    APPEND("bool set_component(Memory *m, size_t entity_id, %s "
           "component);\n",
           structs[i].name);
  }
}

void serializer_include(struct_input *, size_t, char *output, size_t *o,
                        size_t size) {
  APPEND("#include \"components.h\"\n"
         "#include <toml.h>\n"
         "#include \"memory.h\"\n"
         "\n");
  APPEND("#ifdef _WIN32\n"
         "#define strcasecmp _stricmp\n"
         "#endif\n\n");
}

void load_level_source(struct_input *structs, size_t structs_count,
                       char *output, size_t *o, size_t size) {
  APPEND(
      "void ecs_load_level(game *g, const char *sceneFilePath) {\n"
      "\tFILE *fp;\n"
      "\tchar errbuf[200];\n"
      "\n"
      "\tfopen_s(&fp,sceneFilePath, \"r\");\n"
      "\tif (!fp) {\n"
      "\t\tstrerror_s(errbuf, sizeof(errbuf), errno);\n"
      "\t\tprintf(\"Cannot open %%s - %%s\\n\", sceneFilePath, errbuf);\n"
      "\t\treturn;\n"
      "\t}\n"
      "\ttoml_table_t *level = toml_parse_file(fp, errbuf, sizeof(errbuf));\n"
      "\tfclose(fp);\n"
      "\tWorld *w = get_world(g);\n"
      "\tMemory *m = get_header(g);\n"
      "\tfor (int i = 0;; i++) {\n"
      "\t\tconst char *friendly_name = toml_key_in(level, i);\n"
      "\t\tif (!friendly_name)\n"
      "\t\t\tbreak;\n"
      "\t\tsize_t entity = create_entity(w);\n"
      "\t\tset_entity_name(w, entity, friendly_name);\n"
      "\t\ttoml_table_t *attributes = toml_table_in(level, friendly_name);\n"
      "\t\tif (!attributes)\n"
      "\t\t\treturn;\n"
      "\t\tfor (int j = 0;; j++) {\n"
      "\t\t\tconst char *type_key = toml_key_in(attributes, j);\n"
      "\t\t\tif (!type_key)\n"
      "\t\t\t\tbreak;\n"
      "\t\t\ttoml_table_t *nt = toml_table_in(attributes, type_key);\n"
      "\t\t\tif (!nt)\n"
      "\t\t\t\treturn;\n"
      "\t\t\tswitch (mapStringToComponentType(type_key)) {\n");

  for (size_t i = 0; i < structs_count; i++) {
    APPEND("\t\t\tcase %sType: {\n", structs[i].name);

    bool has_path = false;
    size_t path_index = 0;
    for (size_t j = 0; j < structs[i].field_count; j++) {
      if (structs[i].fields[j].type == path_type) {
        has_path = true;
        path_index = j;
        if (j != 0) {
          error("Path field must be first in component", structs[i].name);
          return;
        }
        break;
      }
    }

    // If we have a path field, only load that and skip other fields
    if (has_path) {
      APPEND("\t\t\t\t%s c {\n", structs[i].name);
      APPEND("\t\t\t\t\t .%s = toml_string_in(nt, \"%s\").u.s\n",
             structs[i].fields[0].name, structs[i].fields[0].name);
      APPEND("\t\t\t\t};\n");
      APPEND("\t\t\t\tload_%s(m,entity,&c);\n"
             "\t\t\t\tbreak;\n"
             "\t\t\t}\n",
             structs[i].fields[0].name);
      continue;
    }

    for (size_t j = 0; j < structs[i].field_count; j++) {
      switch (structs[i].fields[j].type) {
      case glmv3_type:
        APPEND("\t\t\t\t\t toml_table_t* vec_%s = toml_table_in(nt,\"%s\");\n",
               structs[i].fields[j].name, structs[i].fields[j].name);
        break;
      default:
        // no need to reload table
        break;
      }
    }

    APPEND("\t\t\t\t%s c {\n", structs[i].name);
    for (size_t j = 0; j < structs[i].field_count; j++) {
      char *separator = (j == structs[i].field_count - 1) ? "" : ",";
      char *type_serializer;
      switch (structs[i].fields[j].type) {
      case path_type:
        APPEND("\t\t\t\t\t .%s = static_cast<load string>(toml_string(nt, "
               "\"%s\").u.d),\n",
               structs[i].fields[j].name, structs[i].fields[j].name);
        break;
      case float_type:

        APPEND("\t\t\t\t\t .%s = static_cast<float>(toml_double_in(nt, "
               "\"%s\").u.d),\n",
               structs[i].fields[j].name, structs[i].fields[j].name);

        break;
      case glmv3_type:
        APPEND("\t\t\t\t\t .%s = glm::vec3(\n"
               "\t\t\t\t\t\t\tstatic_cast<float>(toml_double_in(vec_%s,\"x\")."
               "u.d),\n"
               "\t\t\t\t\t\t\tstatic_cast<float>(toml_double_in(vec_%s,\"y\")."
               "u.d),\n"
               "\t\t\t\t\t\t\tstatic_cast<float>(toml_double_in(vec_%s,\"z\")."
               "u.d)),\n",
               structs[i].fields[j].name, structs[i].fields[j].name,
               structs[i].fields[j].name, structs[i].fields[j].name);
        break;
      case gluint_type:
        APPEND("\t\t\t\t\t .%s = static_cast<GLuint>(toml_int_in(nt, "
               "\"%s\").u.i),\n",
               structs[i].fields[j].name, structs[i].fields[j].name);
        break;
      case size_t_type:
        APPEND("\t\t\t\t\t .%s = static_cast<size_t>(toml_int_in(nt, "
               "\"%s\").u.i),\n",
               structs[i].fields[j].name, structs[i].fields[j].name);
        break;
      case submeshp_type:
        break;
      default:
        type_serializer = "";
        error("Unsupported field type: ", structs[i].fields[j].name);
        break;
      }
    }
    APPEND("\t\t\t\t};\n");
    APPEND("\t\t\t\tadd_component(m,entity,c);\n"
           "\t\t\t\tbreak;\n"
           "\t\t\t}\n");
  }
  APPEND("\t\t\tcase UNKNOWN_TYPE: {\n"
         "\t\t\t\tprintf(\"UNKNOWN_TYPE: %%s\\n\",type_key);\n"
         "\t\t\t\tbreak;\n"
         "\t\t\t}\n");

  APPEND("\t\t\t}\n"
         "\t\t}\n"
         "\t}\n"
         "}\n");
}

void save_level_source(struct_input *structs, size_t structs_count,
                       char *output, size_t *o, size_t size) {
  APPEND("void save_level(Memory *m, const char *saveFilePath) {\n"
         "\tFILE *fp;\n"
         "\tfopen_s(&fp,saveFilePath, \"w\");\n"
         "\tif (!fp) {\n"
         "\t\tprintf(\"Failed to open file %%s for writing.\\n\", "
         "saveFilePath);\n"
         "\t\treturn;\n"
         "\t}\n"
         "\n"
         "\tWorld *w = &m->world;\n"
         "\tfor (size_t i = 0; i < w->entity_count; ++i) {\n"
         "\t\tsize_t entity_id = w->entity_ids[i];\n"
         "\t\tuint32_t mask = w->component_masks[entity_id];\n");

  for (size_t i = 0; i < structs_count; i++) {
    APPEND("\t\tif(mask & %sComponent) {\n", structs[i].name);
    size_t name_len = strlen(structs[i].name) + 1;
    char lowerName[name_len];
    strcpy_s(lowerName, name_len, structs[i].name);
    lowerName[0] = tolower(lowerName[0]);
    APPEND("\t\t\t%s %s;\n", structs[i].name, lowerName);
    APPEND("\t\t\tif (get_component(m, entity_id, &%s)) {\n", lowerName);

    bool has_path = false;
    size_t path_index = 0;
    for (size_t j = 0; j < structs[i].field_count; j++) {
      if (structs[i].fields[j].type == path_type) {
        has_path = true;
        path_index = j;
        if (j != 0) {
          error("Path field must be first in component", structs[i].name);
          return;
        }
        break;
      }
    }
    if (has_path) {
      APPEND("\t\t\t\tfprintf(fp,\"%s = { %s = %%s }\\n\", %s.%s);\n",
             lowerName, structs[i].fields[0].name, lowerName,
             structs[i].fields[0].name);
      APPEND("\t\t\t}\n\t\t}\n");
      continue; // Skip to next component
    }

    APPEND("\t\t\t\tfprintf(fp,\"%s = { ", lowerName);
    for (size_t j = 0; j < structs[i].field_count; j++) {
      char *separator = (j == structs[i].field_count - 1) ? "" : ",";
      char *type_serializer;
      switch (structs[i].fields[j].type) {
      case float_type:
        type_serializer = "%.2f";
        APPEND("%s = %%.2f%s ", structs[i].fields[j].name, separator);
        break;
      case glmv3_type:
        APPEND("%s = { x = %%.2f, y= %%.2f, z= %%.2f }%s ",
               structs[i].fields[j].name, separator);
        break;
      case gluint_type:
        APPEND("%s = %%u%s ", structs[i].fields[j].name, separator);
        break;
      case size_t_type:
        APPEND("%s = %%zu%s ", structs[i].fields[j].name, separator);
        break;
      case submeshp_type:
        break;
      default:
        type_serializer = "";
        error("Unsupported field type: ", structs[i].fields[j].name);
        break;
      }
    }
    APPEND("}\",");
    for (size_t j = 0; j < structs[i].field_count; j++) {
      char *separator = (j == structs[i].field_count - 1) ? "" : ",";
      switch (structs[i].fields[j].type) {
      case path_type:
        break;
      case glmv3_type:
        size_t total_components = 0;
        for (size_t k = 0; k < structs[i].field_count; k++) {
          total_components += (structs[i].fields[k].type == glmv3_type) ? 3 : 1;
        }
        size_t current_pos = 0;
        for (size_t k = 0; k < j; k++) {
          current_pos += (structs[i].fields[k].type == glmv3_type) ? 3 : 1;
        }

        APPEND(" %s.%s.x%s", lowerName, structs[i].fields[j].name,
               (current_pos + 1 == total_components) ? "" : ",");
        APPEND(" %s.%s.y%s", lowerName, structs[i].fields[j].name,
               (current_pos + 2 == total_components) ? "" : ",");
        APPEND(" %s.%s.z%s", lowerName, structs[i].fields[j].name,
               (current_pos + 3 == total_components) ? "" : ",");
        break;
      case submeshp_type:
        APPEND(" %s.%s.z%s", lowerName, structs[i].fields[j].name,
               (current_pos + 3 == total_components) ? "" : ",");
        break;
      default:
        APPEND(" %s.%s%s", lowerName, structs[i].fields[j].name, separator);
        break;
      }
    }
    APPEND(");\n"
           "\t\t\t}\n"
           "\t\t}\n");
  }

  APPEND("\t\tfprintf(fp, \"\\n\");\n"
         "\t}\n"
         "\tfclose(fp);\n"
         "\tprintf(\"World saved to %%s\\n\", saveFilePath);\n"
         "}\n");
}

void serializer_source(struct_input *structs, size_t structs_count,
                       char *output, size_t *o, size_t size) {

  APPEND("extern size_t component_count;\n"
         "size_t component_count = %zu;\n\n",
         structs_count);

  APPEND("const char *component_names[] = {\n");
  for (size_t i = 0; i < structs_count; i++) {
    APPEND("\t\"%s\",\n", structs[i].name);
  }
  APPEND("};\n\n");

  APPEND("ComponentType mapStringToComponentType(const char * type_key){\n");
  for (size_t i = 0; i < structs_count; i++) {
    APPEND("\tif(strcasecmp(type_key, \"%s\") == 0) return %sType;\n",
           structs[i].name, structs[i].name);
  }
  APPEND("\treturn UNKNOWN_TYPE;\n");
  APPEND("}\n");

  for (size_t i = 0; i < structs_count; i++) {
    APPEND("void add_component(Memory *m, size_t entity_id, %s "
           "component) {\n"
           "\tsize_t i = m->components->p%ss->count;\n"
           "\tm->components->p%ss->entity_ids[i] = entity_id;\n"
           "\tm->components->p%ss->components[i] = component;\n"
           "\tm->components->p%ss->count++;\n"
           "\tm->world.component_masks[entity_id] |= %sComponent;\n"
           "}\n\n",
           structs[i].name, structs[i].name, structs[i].name, structs[i].name,
           structs[i].name, structs[i].name);
  }

  for (size_t i = 0; i < structs_count; i++) {
    APPEND("bool get_component(Memory *m, size_t entity_id, %s "
           "*component) {\n"
           "\tif (!check_entity_component(m, entity_id, %sComponent)) {\n"
           "\t\treturn false;\n"
           "\t}\n"
           "\tfor (size_t i = 0; i < m->components->p%ss->count; i++) {\n"
           "\t\tif (m->components->p%ss->entity_ids[i] == entity_id) {\n"
           "\t\t\t*component = m->components->p%ss->components[i];\n"
           "\t\t\treturn true;\n"
           "\t\t}\n"
           "\t}\n"
           "\treturn false;\n"
           "}\n\n",
           structs[i].name, structs[i].name, structs[i].name, structs[i].name,
           structs[i].name);
  }

  for (size_t i = 0; i < structs_count; i++) {
    APPEND("bool set_component(Memory *m, size_t entity_id, %s "
           "component) {\n"
           "\tif (!check_entity_component(m, entity_id, %sComponent)) {\n"
           "\t\treturn false;\n"
           "\t}\n"
           "\tfor (size_t i = 0; i < m->components->p%ss->count; i++) {\n"
           "\t\tif (m->components->p%ss->entity_ids[i] == entity_id) {\n"
           "\t\t\tm->components->p%ss->components[i] = component;\n"
           "\t\t\treturn true;\n"
           "\t\t}\n"
           "\t}\n"
           "\treturn false;\n"
           "}\n\n",
           structs[i].name, structs[i].name, structs[i].name, structs[i].name,
           structs[i].name);
  }
  load_level_source(structs, structs_count, output, o, size);
  save_level_source(structs, structs_count, output, o, size);
}
