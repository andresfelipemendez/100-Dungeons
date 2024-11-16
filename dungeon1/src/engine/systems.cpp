#include "systems.h"
#include "ecs.h"
#include "ext/matrix_transform.hpp"
#include "fwd.hpp"

#include <cstdio>
#include <glm.hpp>
#include <gtc/constants.hpp>
#include <gtc/matrix_transform.hpp>

#include <glad.h>

#include <GLFW/glfw3.h>

#include <game.h>

void input_system(game *g, MemoryHeader *h) {
	if (!get_entities(h, InputComponent | PositionComponent)) {
		return;
	}

	for (size_t i = 0; i < h->query.count; ++i) {
		size_t entity = h->query.entities[i];

		// Retrieve the current position of the entity
		Position p;
		if (!get_component(h, entity, &p)) {
			continue;
		}

		float speed = 0.1f; // Adjust movement speed as needed

		int joystickID = GLFW_JOYSTICK_1;
		if (glfwJoystickPresent(joystickID)) {
			int count;
			const float *axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &count);
			if (axes && count >= 2) {
				Position p{.x = axes[0], .z = axes[1]};
				set_component(h, entity, p);
			}
		}
		// Update position based on WASD input
		if (glfwGetKey(g->window, GLFW_KEY_W) == GLFW_PRESS) {
			p.z -= speed;
		}
		if (glfwGetKey(g->window, GLFW_KEY_S) == GLFW_PRESS) {
			p.z += speed;
		}
		if (glfwGetKey(g->window, GLFW_KEY_A) == GLFW_PRESS) {
			p.x -= speed;
		}
		if (glfwGetKey(g->window, GLFW_KEY_D) == GLFW_PRESS) {
			p.x += speed;
		}

		// Update the entity's position component with the new position
		// set_component_value(h, entity, &position);
	}
}

void camera_follow_system(game *g, MemoryHeader *h) {
	size_t camera_entity;
	if (!get_entity(h, CameraComponent, camera_entity)) {
		return;
	}

	size_t player_entity;
	if (!get_entity(h, InputComponent, player_entity)) {
		return;
	}

	Position camera_position;
	if (!get_component(h, camera_entity, &camera_position)) {
		return;
	}

	Position player_position;
	if (!get_component(h, player_entity, &player_position)) {
		return;
	}

	camera_position = {
		.x = player_position.x + 1,
		.y = player_position.y + 1,
		.z = player_position.z + 1,
	};
	set_component(h, camera_entity, camera_position);
}

glm::vec4 cc(0.45f, 0.55f, 0.60f, 1.00f);
void rendering_system(MemoryHeader *h) {
	glClearColor(cc.x * cc.a, cc.y * cc.a, cc.z * cc.a, cc.a);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	size_t camera_entity;
	if (!get_entity(h, CameraComponent, camera_entity)) {
		printf("couldn't find camera entity\n");
	}

	Position p;
	if (!get_component(h, camera_entity, &p)) {
		return;
	}

	glm::vec3 cameraPosition = glm::vec3(p.x, p.y, p.z);
	glm::vec3 targetPosition = glm::vec3(1.0f, 0.0f, 0.0f);
	glm::vec3 upDirection = glm::vec3(0.0f, 1.0f, 0.0f);

	glm::mat4 view = glm::lookAt(cameraPosition, targetPosition, upDirection);

	float aspectRatio = 16.0f / 9.0f;

	Camera c;
	if (!get_component(h, camera_entity, &c)) {
		printf("couldn't find camera entity\n");
	}
	glm::mat4 projection =
		glm::perspective(glm::radians(c.fov), aspectRatio, c.near, c.far);
	glm::mat4 viewProj = projection * view;

	if (!get_entities(h,
					  MaterialComponent | ModelComponent | PositionComponent)) {
		return;
	}

	for (size_t i = 0; i < h->query.count; ++i) {
		Material material;
		if (get_component(h, h->query.entities[i], &material)) {
			glUseProgram(material.shader_id);
		}
		glm::mat4 worldTransform = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));

		Position p;
		if (get_component(h, h->query.entities[i], &p)) {
			GLuint view_loc =
				glGetUniformLocation(material.shader_id, "uViewProj");
			glUniformMatrix4fv(view_loc, 1, GL_TRUE, &viewProj[0][0]);

			worldTransform =
				glm::translate(worldTransform, glm::vec3{p.x, p.y, p.z});
			GLuint loc =
				glGetUniformLocation(material.shader_id, "uWorldTransform");
			glUniformMatrix4fv(loc, 1, GL_TRUE, &worldTransform[0][0]);
		}

		Model m;
		if (get_component(h, h->query.entities[i], &m)) {
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m.drawsBuffer);
			for (auto i = 0U; i < m.submesh_count; ++i) {
				auto &submesh = m.submeshes[i];
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

void systems(game *g, MemoryHeader *h) {
	input_system(g, h);
	camera_follow_system(g, h);
	rendering_system(h);
}