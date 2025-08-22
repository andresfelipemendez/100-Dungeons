#include "utest.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ecs.h"
#include "memory.h"


const char* TEST_FILE_PATH = "ecs_test_output.toml";

// Mock function to simulate saving to a buffer
void save_level_to_buffer_test() {
    
    game g;
	g.world = malloc(100 * 1024);
	if (g.world != NULL)
	{
		memset(g.world, 0, 100 * 1024); // Set all allocated memory to zero
	}
	init_engine_memory(&g);

    FILE* fp = fopen(TEST_FILE_PATH, "r");
    ASSERT_TRUE(fp != nullptr);
}
