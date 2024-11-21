#include "codegenerator.h"
#include "utest.h"

UTEST(foo, bar) { ASSERT_TRUE(1); }
UTEST(code_generator, generates_correct_output_with_buffers) {
	const char *input_data = "Line1\nLine2\nLine3\n";
	char output_buffer[1024] = {0};

	generate_code_from_buffers(input_data, output_buffer,
							   sizeof(output_buffer));

	const char *expected_output =
		"Processed: Line1\nProcessed: Line2\nProcessed: Line3\n";
	ASSERT_STREQ(output_buffer, expected_output);
}
UTEST_MAIN()
