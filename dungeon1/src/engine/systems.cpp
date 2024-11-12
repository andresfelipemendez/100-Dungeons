#include "systems.h"
#include "ecs.h"

#include <cstdio>
#include <glm.hpp>
#include <gtc/constants.hpp>
#include <gtc/matrix_transform.hpp>

#include <glad.h>

void systems(MemoryHeader *h) { rendering_system(h); }

glm::vec4 cc(0.45f, 0.5f, 0.60f, 1.00f);
void rendering_system(MemoryHeader *h) {
	glClearColor(cc.x * cc.a, cc.y * cc.a, cc.z * cc.a, cc.a);
	glClear(GL_COLOR_BUFFER_BIT);

	size_t camera_entity;
	if (!get_entity(h, CAMERA_COMPONENT, camera_entity)) {
		printf("couldn't find camera entity\n");
	}

	if (!get_entities(h, MATERIAL_COMPONENT | MODEL_COMPONENT |
							 TEXTURE_COMPONENT | POSITION_COMPONENT)) {
		return;
	}

	for (size_t i = 0; i < h->query.count; ++i) {

		Material material;
		if (get_component_value(h, h->query.entities[i], &material)) {
			glUseProgram(material.shader_id);
		}

		StaticMesh staticMesh;
		if (get_component_value(h, h->query.entities[i], &staticMesh)) {
			// glUseProgram(material.shader_id);
		}

		// Models
	}
	// glUseProgram(shaderProgram);

	//----------------------------------------------------
	// Camera camera;
	// if (!get_component_value(h, camera_entity, CAMERA_COMPONENT, camera)) {
	// 	printf("couldn't find camera entity\n");
	// } else {
	// 	GLuint loc = glGetUniformLocation(shaderProgram, "uViewProj");
	// 	if (loc != -1) {
	// 		glUniformMatrix4fv(loc, 1, GL_FALSE, &camera.projection[0][0]);
	// 	} else {
	// 		printf("Uniform 'uViewProj' not found in shader program\n");
	// 	}
	// }

	// Vec3 camera_position;
	// if (!get_component_value(h, camera_entity, POSITION_COMPONENT,
	// 						 &camera_position)) {
	// 	printf("couldn't find camera position\n");
	// } else {
	// 	glm::mat4 worldTransform =
	// 		glm::scale(glm::mat4(1.0f), glm::vec3(1.0f)); // Scale of 1
	// 	worldTransform = glm::translate(
	// 		worldTransform, glm::vec3(camera_position.x, camera_position.y,
	// 								  camera_position.z)); // Apply translation

	// 	GLuint loc = glGetUniformLocation(shaderProgram, "uWorldTransform");
	// 	if (loc != -1) {
	// 		glUniformMatrix4fv(loc, 1, GL_FALSE, &worldTransform[0][0]);
	// 	} else {
	// 		printf("Uniform 'uWorldTransform' not found in shader program\n");
	// 	}
	// }

	// // Set up world transformation for the mesh
	// Vec3 mesh_pos = {0.0f, 0.0f, 0.0f};
	// glm::mat4 worldTransform = glm::translate(
	// 	glm::mat4(1.0f), glm::vec3(mesh_pos.x, mesh_pos.y, mesh_pos.z));

	// // Set the uWorldTransform matrix in the shader
	// GLuint loc = glGetUniformLocation(shaderProgram, "uWorldTransform");
	// if (loc != -1) {
	// 	glUniformMatrix4fv(loc, 1, GL_FALSE, &worldTransform[0][0]);
	// } else {
	// 	printf("Uniform 'uWorldTransform' not found in shader program\n");
	// }

	// glBindVertexArray(VAO);
	// glDrawArrays(GL_TRIANGLES, 0, 3);
	// glBindVertexArray(0);

	// for (size_t i = 0; i < h->meshes->count; ++i) {

	// 	StaticMesh mesh = h->meshes->mesh_data[i];
	// 	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, mesh.drawsBuffer);
	// 	for (auto i = 0U; i < mesh.submesh_count; ++i) {
	// 		auto &submesh = mesh.submeshes[i];
	// 		glBindVertexArray(submesh.vertexArray);
	// 		glDrawElementsIndirect(
	// 			GL_TRIANGLES, submesh.indexType,
	// 			reinterpret_cast<const void *>(i * sizeof(SubMesh)));
	// 	}
	// }
}