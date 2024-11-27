#include "arena.h"
#include "codegenerator.h"
#include "utest.h"

#include <toml.h>

#define output_size 1024

UTEST(code_generator, two_structs) {

  const char *input_data = "[Position]\n"
                           "x = 'float'\n"
                           "y = 'float'\n"
                           "z = 'float'\n"
                           "[Rotation]\n"
                           "x = 'float'\n";
  char errbuf[200];
  toml_table_t *conf = toml_parse((char *)input_data, errbuf, sizeof(errbuf));
  ASSERT_TRUE(conf != NULL);

  Arena *structs_arena = arena_create(1024);
  Arena *strings_arena = arena_create(1024);
  struct_input *structs = NULL;
  size_t structs_count = generate_struct_data_structure(
      conf, structs_arena, strings_arena, &structs);
  ASSERT_EQ(structs_count, (size_t)2);
  ASSERT_STREQ(structs[0].name, "Position");
  ASSERT_EQ(structs[0].field_count, (size_t)3);
  ASSERT_STREQ(structs[0].fields[0].name, "x");

  ASSERT_STREQ(structs[1].name, "Rotation");
  ASSERT_EQ(structs[1].field_count, (size_t)1);
  ASSERT_STREQ(structs[1].fields[0].name, "x");
  arena_destroy(structs_arena);
  arena_destroy(strings_arena);
}

UTEST(code_generator, structs) {

  const char *input_data = "[Position]\n"
                           "x = 'float'\n"
                           "y = 'float'\n"
                           "z = 'float'\n";
  char errbuf[200];
  toml_table_t *conf = toml_parse((char *)input_data, errbuf, sizeof(errbuf));
  ASSERT_TRUE(conf != NULL);

  Arena *structs_arena = arena_create(1024);
  Arena *strings_arena = arena_create(1024);
  struct_input *structs = NULL;
  size_t structs_count = generate_struct_data_structure(
      conf, structs_arena, strings_arena, &structs);
  ASSERT_EQ(structs_count, (size_t)1);
  ASSERT_STREQ(structs[0].name, "Position");
  ASSERT_EQ(structs[0].field_count, (size_t)3);
  ASSERT_STREQ(structs[0].fields[0].name, "x");
  arena_destroy(structs_arena);
  arena_destroy(strings_arena);
}

UTEST(code_generator, generates_correct_output_with_buffers) {
  field fields[] = {{.name = "x", .type = float_type},
                    {.name = "y", .type = float_type},
                    {.name = "z", .type = float_type}};

  struct_input structs[] = {
      {.name = "Position", .fields = fields, .field_count = 3}};

  char output_buffer[output_size] = {0};
  size_t o = 0;
  gen_struct_definitions(structs, 1, output_buffer, &o, output_size);

  const char *expected_output = "struct Position {\n"
                                "\tfloat x;\n"
                                "\tfloat y;\n"
                                "\tfloat z;\n"
                                "};\n";
  ASSERT_STREQ(expected_output, output_buffer);
}

UTEST(code_generator, serializer) {
  field fields[] = {{.name = "x", .type = float_type},
                    {.name = "y", .type = float_type},
                    {.name = "z", .type = float_type}};

  struct_input structs[] = {
      {.name = "Position", .fields = fields, .field_count = 3}};

  char output_buffer[output_size] = {0};
  size_t o = 0;
  serializer_source(structs, 1, output_buffer, &o, output_size);

  const char *expected_output =
      "if(mask & PositionComponent) {\n"
      "\tPosition position;\n"
      "\tif (get_component(h->components, entity_id, &position)) {\n"
      "\t\tfprintf(fp,\"position = { x = %.2f, y = %.2f, z = %.2f }\", "
      "position.x, position.y, position.z);\n"
      "\t}\n"
      "}\n";
  ASSERT_STREQ(expected_output, output_buffer);
}

UTEST(code_generator, serializer_include) {
  char output_buffer[output_size] = {0};
  size_t o = 0;
  serializer_include(NULL, 0, output_buffer, &o, output_size);
  printf("output: %s\n",output_buffer);
  const char *expected_output = "#include \"components.h\"\n";
  ASSERT_STREQ(expected_output, output_buffer);
}

UTEST_MAIN()
