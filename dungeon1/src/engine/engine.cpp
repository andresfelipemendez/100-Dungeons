#include "engine.h"
#include "fwd.hpp"
#include "memory.h"
#include <externals.h>
#include <game.h>
#include <stdio.h>

#include "asset_loader.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "ImGuizmo.h"

#include "ecs.h"
#include "systems.h"

#include <GLFW/glfw3.h>
#include <glad.h>

#include <glm.hpp>
#include <gtc/constants.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

unsigned int VAO, VBO, shaderProgram;

EXPORT void load_level(game *g, const char *sceneFilePath) {}

static void glfw_error_callback(int error, const char *description) {
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

EXPORT void init_engine(game *g) { init_engine_memory(g); }

EXPORT void init_engine_renderer(game *g) {}

EXPORT void load_meshes(game *g) {
	MemoryHeader *h = get_header(g);
	World *w = get_world(g);

	reset_memory(h);

	load_shaders(g);
	const char *sceneFilePath = "assets\\scene.toml";

	ecs_load_level(g, sceneFilePath);
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

EXPORT void update(game *g) {
	MemoryHeader *h = get_header(g);
	systems(g, h);

	draw_opengl(g);
}

EXPORT void draw_opengl(game *g) { MemoryHeader *h = get_header(g); }

// Helper function to handle transformations with ImGuizmo
void EditTransform(float *cameraView, float *cameraProjection, float *matrix,
				   bool editTransformDecomposition) {

	ImGuiIO &io = ImGui::GetIO();

	static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
	static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

	ImGuizmo::Manipulate(cameraView, cameraProjection, mCurrentGizmoOperation,
						 mCurrentGizmoMode, matrix, NULL, NULL);
}

EXPORT void hotreloadable_imgui_draw(game *g) {
	// Set the ImGui context and allocator functions
	ImGui::SetCurrentContext(g->ctx);
	ImGui::SetAllocatorFunctions(g->alloc_func, g->free_func, g->user_data);

	// Start a new ImGui frame if not already done in your main loop
	// ImGui::NewFrame(); // Uncomment if necessary

	// Set the ImGui context for ImGuizmo and begin the frame
	ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
	ImGuizmo::BeginFrame();

	// Begin your ImGui window
	ImGui::Begin("Editor");

	// Set the viewport rectangle for ImGuizmo
	ImVec2 viewportPos = ImVec2(0, 0);
	ImVec2 viewportSize = ImGui::GetIO().DisplaySize;
	ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x,
					  viewportSize.y);

	World *w = get_world(g);
	MemoryHeader *h = get_header(g);
	size_t count = w->entity_count;

	static size_t selected_entity = SIZE_MAX;

	if (ImGui::Button("Save World")) {
		const char *saveFilePath = "scene.toml";
		save_level(h, saveFilePath);
		ImGui::Text("World saved to %s", saveFilePath);
	}

	if (ImGui::Button("Add Entity")) {
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

		if (w->component_masks[selected_entity] & PositionComponent) {

			Position position;
			get_component(h, selected_entity, &position);

			size_t camera_entity;
			if (!get_entity(h, CameraComponent, camera_entity)) {
				printf("couldn't find camera entity\n");
			}

			Position p;
			if (!get_component(h, camera_entity, &p)) {
				return;
			}
			Camera c;
			if (!get_component(h, camera_entity, &c)) {
				printf("couldn't find camera entity\n");
			}
			if (selected_entity != camera_entity) {

				glm::vec3 cameraPosition = glm::vec3(p.x, p.y, p.z);
				glm::vec3 targetPosition = glm::vec3(1.0f, 0.0f, 0.0f);
				glm::vec3 upDirection = glm::vec3(0.0f, 1.0f, 0.0f);

				glm::mat4 view =
					glm::lookAt(cameraPosition, targetPosition, upDirection);

				float aspectRatio = 16.0f / 9.0f;

				glm::mat4 projection = glm::perspective(
					glm::radians(70.0f), aspectRatio, 0.01f, 1000.0f);

				glm::mat4 modelMatrix = glm::translate(
					glm::mat4(1.0f),
					glm::vec3(position.x, position.y, position.z));

				// Call your EditTransform function
				EditTransform(glm::value_ptr(view), glm::value_ptr(projection),
							  glm::value_ptr(modelMatrix), true);

				// Update position based on the manipulated model matrix
				position.x = modelMatrix[3][0];
				position.y = modelMatrix[3][1];
				position.z = modelMatrix[3][2];
				set_component(h, selected_entity, position);

				if (ImGui::InputFloat3("Position", (float *)&position)) {
					set_component(h, selected_entity, position);
					modelMatrix = glm::translate(
						glm::mat4(1.0f),
						glm::vec3(position.x, position.y, position.z));
				}
			} else {
				if (ImGui::InputFloat3("Camera Position", (float *)&position)) {
					set_component(h, selected_entity, position);
				}
			}
		}
	}

	// End your ImGui window
	ImGui::End();

	// Render ImGui if not already done in your main loop
	// ImGui::Render(); // Uncomment if necessary
}
