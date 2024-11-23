#include "arena.h"
#include "codegenerator.h"
#include "utest.h"

#include <toml.h>

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
  // ASSERT_STREQ(structs[0].name, "Position");
  //  ASSERT_EQ(structs[0].field_count, (size_t)3);
  //  ASSERT_STREQ(structs[0].fields[0].name, "x");

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

  const char *input_data = "[Position]\n"
                           "x = 'float'\n"
                           "y = 'float'\n"
                           "z = 'float'\n";

  char output_buffer[1024] = {0};

  generate_code_from_buffers(input_data, output_buffer, sizeof(output_buffer));

  const char *expected_output = "struct Position {\n"
                                "\tfloat x;\n"
                                "\tfloat y;\n"
                                "\tfloat z;\n"
                                "};\n";
  ASSERT_STREQ(expected_output, output_buffer);
}
UTEST_MAIN()
