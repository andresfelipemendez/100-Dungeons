#include "systems.h"
#include "ecs.h"
#include "ext/matrix_transform.hpp"
#include "fwd.hpp"

#include <cstdio>
#include <glm.hpp>
#include <gtc/constants.hpp>
#include <gtc/matrix_transform.hpp>

#include <glad.h>

void systems(MemoryHeader *h) { rendering_system(h); }

glm::vec4 cc(0.45f, 0.55f, 0.60f, 1.00f);
void rendering_system(MemoryHeader *h) {
	glClearColor(cc.x * cc.a, cc.y * cc.a, cc.z * cc.a, cc.a);
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

	size_t camera_entity;
	if (!get_entity(h, CAMERA_COMPONENT, camera_entity)) {
		printf("couldn't find camera entity\n");
	}
	Camera camera;
	if (!get_component_value(h, camera_entity, &camera)) {
		printf("couldn't find camera entity\n");
	}

	if (!get_entities(h, MATERIAL_COMPONENT | MODEL_COMPONENT |
							 POSITION_COMPONENT)) {
		return;
	}

	for (size_t i = 0; i < h->query.count; ++i) {
		Material material;
		if (get_component_value(h, h->query.entities[i], &material)) {
			glUseProgram(material.shader_id);
		}
		glm::mat4 worldTransform = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));

		Vec3 position;
		if (get_component_value(h, h->query.entities[i], &position)) {
			GLuint view_loc =
				glGetUniformLocation(material.shader_id, "uViewProj");
			glUniformMatrix4fv(view_loc, 1, GL_TRUE, &camera.projection[0][0]);

			worldTransform = glm::translate(
				worldTransform, glm::vec3{position.x, position.y, position.z});
			GLuint loc =
				glGetUniformLocation(material.shader_id, "uWorldTransform");
			glUniformMatrix4fv(loc, 1, GL_TRUE, &worldTransform[0][0]);
		}

		StaticMesh staticMesh;
		if (get_component_value(h, h->query.entities[i], &staticMesh)) {
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, staticMesh.drawsBuffer);
			for (auto i = 0U; i < staticMesh.submesh_count; ++i) {
				auto &submesh = staticMesh.submeshes[i];
				glBindVertexArray(submesh.vertexArray);
				glDrawElementsIndirect(
					GL_TRIANGLES, submesh.indexType,
					reinterpret_cast<const void *>(i * sizeof(SubMesh)));
			}
		}
	}
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR) {
		printf("OpenGL error: %d\n", err);
	}

}