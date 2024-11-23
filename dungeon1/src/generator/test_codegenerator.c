#include "codegenerator.h"
#include "utest.h"

UTEST(code_generator, generates_correct_output_with_buffers) {

	const char *input_data = "[Position]\n"
							 "x = 'float'\n"
							 "y = 'float'\n"
							 "z = 'float'\n";

	char output_buffer[1024] = {0};

	generate_code_from_buffers(input_data, output_buffer,
							   sizeof(output_buffer));

	const char *expected_output = "struct Position {\n"
								  "\tfloat x;\n"
								  "\tfloat y;\n"
								  "\tfloat z;\n"
								  "};\n";
	ASSERT_STREQ(expected_output, output_buffer);
}
UTEST_MAIN()
