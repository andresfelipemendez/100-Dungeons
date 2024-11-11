#include "engine.h"
#include "fwd.hpp"
#include "memory.h"
#include <externals.h>
#include <fstream>
#include <game.h>
#include <stdio.h>

#include "asset_loader.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "ecs.h"

#include <GLFW/glfw3.h>
#include <glad.h>

#include <glm.hpp>
#include <gtc/constants.hpp>
#include <gtc/matrix_transform.hpp>

unsigned int VAO, VBO, shaderProgram;

EXPORT void load_shader(game *g) {}

EXPORT void load_level(game *g, const char *sceneFilePath) {
	printf("previously load level at path %s\n", sceneFilePath);
	// ecs_load_level(g, sceneFilePath);
}

EXPORT void init_engine(game *g) { init_engine_memory(g); }

EXPORT void load_meshes(game *g) {

	((MemoryHeader *)g->world)->meshes->count = 0;
	((World *)g->world)->entity_count = 0;

	const char *sceneFilePath = "assets\\scene.toml";
	printf("load level at path %s\n", sceneFilePath);
	ecs_load_level(g, sceneFilePath);

	float vertices[] = {0.0f, 0.5f, 0.0f,  -0.5f, -0.5f,
						0.0f, 0.5f, -0.5f, 0.0f};

	glCreateVertexArrays(1, &VAO);
	glCreateBuffers(1, &VBO);

	glNamedBufferData(VBO, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexArrayVertexBuffer(VAO, 0, VBO, 0, 3 * sizeof(float));
	glEnableVertexArrayAttrib(VAO, 0);
	glVertexArrayAttribFormat(VAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribBinding(VAO, 0, 0);

	const char *vertexShaderSource = "#version 450 core\n"
									 "uniform mat4 uWorldTransform;\n"
									 "uniform mat4 uViewProj;\n"
									 "layout (location = 0) in vec3 aPos;\n"
									 "out vec3 fragWorldPos;\n"
									 "void main()\n"
									 "{\n"
									 "vec4 pos = vec4(aPos, 1.0);\n"
									 "pos = pos * uWorldTransform;\n"
									 "fragWorldPos = pos.xyz;"
									 "gl_Position = pos * uViewProj;\n"
									 "}\0";

	const char *fragmentShaderSource =
		"#version 450 core\n"
		"out vec4 FragColor;\n"
		"uniform vec3 uCameraPos;\n"
		"void main()\n"
		"{\n"
		"   FragColor = vec4(1.0, 0.1, 0.2, 1.0);\n"
		"}\0";

	shaderProgram =
		createShaderProgram(vertexShaderSource, fragmentShaderSource);
}

static void glfw_error_callback(int error, const char *description) {
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

EXPORT void begin_frame(game *g) {
	glfwSetErrorCallback(glfw_error_callback);
	glfwMakeContextCurrent(g->window);
	if (!glfwGetCurrentContext()) {
		fprintf(stderr, "No current OpenGL context detected in DLL\n");
		return;
	}

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		printf("Failed to initialize GLAD in DLL\n");
		return;
	}
}

ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
EXPORT void update(game *g) {
	glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
				 clear_color.z * clear_color.w, clear_color.w);
	glClear(GL_COLOR_BUFFER_BIT);
	draw_opengl(g);
}

EXPORT void draw_opengl(game *g) {
	MemoryHeader *h = get_header(g);
	size_t camera_entity;
	if (!get_entity(h, COMPONENT_CAMERA, camera_entity)) {
		printf("couldn't find camera entity\n");
	}

	glUseProgram(shaderProgram);

	Camera camera;
	if (!get_component_value(h, camera_entity, COMPONENT_CAMERA, camera)) {
		printf("couldn't find camera entity\n");
	} else {
		GLuint loc = glGetUniformLocation(shaderProgram, "uViewProj");
		if (loc != -1) {
			glUniformMatrix4fv(loc, 1, GL_FALSE, &camera.projection[0][0]);
		} else {
			printf("Uniform 'uViewProj' not found in shader program\n");
		}
	}

	Vec3 camera_position;
	if (!get_component_value(h, camera_entity, COMPONENT_POSITION,
							 &camera_position)) {
		printf("couldn't find camera position\n");
	} else {
		glm::mat4 worldTransform =
			glm::scale(glm::mat4(1.0f), glm::vec3(1.0f)); // Scale of 1
		worldTransform = glm::translate(
			worldTransform, glm::vec3(camera_position.x, camera_position.y,
									  camera_position.z)); // Apply translation

		GLuint loc = glGetUniformLocation(shaderProgram, "uWorldTransform");
		if (loc != -1) {
			glUniformMatrix4fv(loc, 1, GL_FALSE, &worldTransform[0][0]);
		} else {
			printf("Uniform 'uWorldTransform' not found in shader program\n");
		}
	}

	// Set up world transformation for the mesh
	Vec3 mesh_pos = {0.0f, 0.0f, 0.0f};
	glm::mat4 worldTransform = glm::translate(
		glm::mat4(1.0f), glm::vec3(mesh_pos.x, mesh_pos.y, mesh_pos.z));

	// Set the uWorldTransform matrix in the shader
	GLuint loc = glGetUniformLocation(shaderProgram, "uWorldTransform");
	if (loc != -1) {
		glUniformMatrix4fv(loc, 1, GL_FALSE, &worldTransform[0][0]);
	} else {
		printf("Uniform 'uWorldTransform' not found in shader program\n");
	}

	glBindVertexArray(VAO);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);

	for (size_t i = 0; i < h->meshes->count; ++i) {

		StaticMesh mesh = h->meshes->mesh_data[i];
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, mesh.drawsBuffer);
		for (auto i = 0U; i < mesh.submesh_count; ++i) {
			auto &submesh = mesh.submeshes[i];
			glBindVertexArray(submesh.vertexArray);
			glDrawElementsIndirect(
				GL_TRIANGLES, submesh.indexType,
				reinterpret_cast<const void *>(i * sizeof(SubMesh)));
		}
	}
}

EXPORT void hotreloadable_imgui_draw(game *g) {
	ImGui::SetCurrentContext(g->ctx);
	ImGui::SetAllocatorFunctions(g->alloc_func, g->free_func, g->user_data);
	ImGui::Begin("Hello! world from reloadable dll!");

	World *w = get_world(g);
	MemoryHeader *h = get_header(g);
	size_t count = w->entity_count;

	static size_t selected_entity = SIZE_MAX;

	if (ImGui::Button("Save World")) {
		const char *saveFilePath = "ecs_world_save.toml";
		save_level(h, saveFilePath);
		ImGui::Text("World saved to %s", saveFilePath);
	}

	if (ImGui::Button("add entity")) {
		selected_entity = create_entity(w);
	}

	ImGui::Text("Entity List:");
	for (size_t i = 0; i < count; ++i) {
		if (ImGui::Selectable(w->entity_names[i], selected_entity == i)) {
			selected_entity = i;
		}
	}

	ImGui::Separator();

	if (selected_entity != SIZE_MAX) {
		ImGui::Text("Inspecting Entity: %s", w->entity_names[selected_entity]);

		static int selected_component = -1;

		if (ImGui::BeginCombo("Add Component", "Select Component")) {
			for (int i = 0; i < component_count; ++i) {
				if (ImGui::Selectable(component_names[i],
									  selected_component == i)) {
					selected_component = i;
					add_component(h, selected_entity, (1 << i));
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Separator();
		if (w->component_masks[selected_entity]) {
			ImGui::Text("Components:");
		}
		if (w->component_masks[selected_entity] & COMPONENT_POSITION) {
			Vec3 position;

			get_component_value(h, selected_entity, COMPONENT_POSITION,
								&position);
			if (ImGui::InputFloat3("Position", (float *)&position)) {
				set_component_value(h, selected_entity, COMPONENT_POSITION,
									position);
			}
		}
		if (w->component_masks[selected_entity] & COMPONENT_CAMERA) {
			Vec3 camera;

			get_component_value(h, selected_entity, COMPONENT_POSITION,
								&camera);
			if (ImGui::InputFloat3("Camera", (float *)&camera)) {
				set_component_value(h, selected_entity, COMPONENT_POSITION,
									camera);
			}
		}
	}

	ImGui::End();
}
