#include "systems.h"
#include "ecs.h"

#include <glm.hpp>
#include <gtc/constants.hpp>
#include <gtc/matrix_transform.hpp>

#include <glad.h>

void systems(MemoryHeader *h) {
	rendering_system(h);
	// glClearColor(clear_color.x * clear_color.w, clear_color.y *
	// clear_color.w, 			 clear_color.z * clear_color.w, clear_color.w);
	// glClear(GL_COLOR_BUFFER_BIT);

	// g->draw_opengl(g);
}

glm::vec4 clear_color(0.45f, 0.55f, 0.60f, 1.00f);
void rendering_system(MemoryHeader *h) {
	glClearColor(clear_color.x * clear_color.a, clear_color.y * clear_color.a,
				 clear_color.z * clear_color.a, clear_color.a);
	glClear(GL_COLOR_BUFFER_BIT);
}