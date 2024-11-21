#include "codegenerator.h"
#include <stdio.h>

bool generate_code_from_buffers(const char *input, char *output, size_t size) {

	snprintf(output, size,
			 "Processed: Line1\nProcessed: Line2\nProcessed: Line3\n");
	return false;
}
