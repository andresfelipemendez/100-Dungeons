#include "codegenerator.h"
#include "utest.h"

UTEST(code_generator, generates_correct_output_with_buffers) {
	// clang-format off
	const char *input_data =
	"struct Position {\n"
	"    float x;\n"
	"    float y;\n"
	"    float z;\n"
	"};\n";
	// clang-format on
	char output_buffer[1024] = {0};

	generate_code_from_buffers(input_data, output_buffer,
							   sizeof(output_buffer));

	const char *expected_output =
		"Processed: Line1\nProcessed: Line2\nProcessed: Line3\n";
	ASSERT_STREQ(expected_output, output_buffer);
}
UTEST_MAIN()
