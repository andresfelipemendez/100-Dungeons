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
#include <ImGuizmo.h>

#include "ecs.h"
#include "systems.h"

#include <GLFW/glfw3.h>
#include <glad.h>

#include <glm.hpp>
#include <gtc/constants.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

unsigned int VAO, VBO, shaderProgram;

EXPORT void load_level(game *g, const char *sceneFilePath) {

}

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
	systems(h);

	draw_opengl(g);
}

EXPORT void draw_opengl(game *g) {
	MemoryHeader *h = get_header(g);

}

// Helper function to handle transformations with ImGuizmo
void EditTransform(float* cameraView, float* cameraProjection, float* matrix, bool editTransformDecomposition) {
	static ImGuizmo::OPERATION mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	static ImGuizmo::MODE mCurrentGizmoMode = ImGuizmo::LOCAL;
	static bool useSnap = false;
	static float snap[3] = { 1.f, 1.f, 1.f };
	static float bounds[] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
	static float boundsSnap[] = { 0.1f, 0.1f, 0.1f };
	static bool boundSizing = false;
	static bool boundSizingSnap = false;

	if (editTransformDecomposition) {
		if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
			mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
		ImGui::SameLine();
		if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
			mCurrentGizmoOperation = ImGuizmo::ROTATE;
		ImGui::SameLine();
		if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
			mCurrentGizmoOperation = ImGuizmo::SCALE;

		float matrixTranslation[3], matrixRotation[3], matrixScale[3];
		ImGuizmo::DecomposeMatrixToComponents(matrix, matrixTranslation, matrixRotation, matrixScale);
		ImGui::InputFloat3("Tr", matrixTranslation);
		ImGui::InputFloat3("Rt", matrixRotation);
		ImGui::InputFloat3("Sc", matrixScale);
		ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, matrix);

		if (mCurrentGizmoOperation != ImGuizmo::SCALE) {
			if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
				mCurrentGizmoMode = ImGuizmo::LOCAL;
			ImGui::SameLine();
			if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
				mCurrentGizmoMode = ImGuizmo::WORLD;
		}

		ImGui::Checkbox("Use Snap", &useSnap);
		if (useSnap) {
			switch (mCurrentGizmoOperation) {
				case ImGuizmo::TRANSLATE: ImGui::InputFloat3("Snap", &snap[0]); break;
				case ImGuizmo::ROTATE: ImGui::InputFloat("Angle Snap", &snap[0]); break;
				case ImGuizmo::SCALE: ImGui::InputFloat("Scale Snap", &snap[0]); break;
			}
		}

		ImGui::Checkbox("Bound Sizing", &boundSizing);
		if (boundSizing) {
			ImGui::Checkbox("Snap Bounds", &boundSizingSnap);
			ImGui::InputFloat3("Bounds Snap", boundsSnap);
		}
	}

	ImGuizmo::SetDrawlist();
	ImGuizmo::SetRect(0, 0, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
	ImGuizmo::Manipulate(cameraView, cameraProjection, mCurrentGizmoOperation, mCurrentGizmoMode, matrix, NULL, useSnap ? &snap[0] : NULL, boundSizing ? bounds : NULL, boundSizingSnap ? boundsSnap : NULL);
}

EXPORT void hotreloadable_imgui_draw(game *g) {
	ImGui::SetCurrentContext(g->ctx);
	ImGui::SetAllocatorFunctions(g->alloc_func, g->free_func, g->user_data);
	ImGui::Begin("Editor");

	World *w = get_world(g);
	MemoryHeader *h = get_header(g);
	size_t count = w->entity_count;

	static size_t selected_entity = SIZE_MAX;

	if (ImGui::Button("Save World")) {
		const char *saveFilePath = "ecs_world_save.toml";
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

		if (w->component_masks[selected_entity] & POSITION_COMPONENT) {
			Vec3 position;
			get_component_value(h, selected_entity, &position);

			glm::vec3 cameraPosition = glm::vec3(-1.5f, 1.0f, 2.0f);
			glm::vec3 targetPosition = glm::vec3(1.0f, 0.0f, 0.0f);
			glm::vec3 upDirection = glm::vec3(0.0f, 1.0f, 0.0f);

			glm::mat4 viewMatrix = glm::lookAt(cameraPosition, targetPosition, upDirection);
			glm::mat4 projectionMatrix = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);

			glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(position.x, position.y, position.z));

			EditTransform(glm::value_ptr(viewMatrix), glm::value_ptr(projectionMatrix), glm::value_ptr(modelMatrix), true);

			position.x = modelMatrix[3][0];
			position.y = modelMatrix[3][1];
			position.z = modelMatrix[3][2];
			set_component_value(h, selected_entity, position);

			if (ImGui::InputFloat3("Position", (float *)&position)) {
				set_component_value(h, selected_entity, position);
				modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(position.x, position.y, position.z));
			}
		}
	}

	ImGui::End();
}
