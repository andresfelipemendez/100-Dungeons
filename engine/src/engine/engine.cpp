#include "engine.h"
#include "fwd.hpp"
#include "memory.h"
#include <cstring>
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

#include "components.h"
#include "memory.h"
#include <glm.hpp>
#include <gtc/constants.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>
#include <gtc/type_ptr.hpp>

unsigned int VAO, VBO, shaderProgram;

EXPORT void load_level(game *g, const char *sceneFilePath) {
    Memory *m = get_header(g);
    World *w = get_world(g);
    reset_memory(m);
    g->load_shaders(g);
    ecs_load_level(g, sceneFilePath);
}

EXPORT void asset_reload(game *g, const char *assetLoaded) {
    printf("asset_reload\n");
    printf("aset to reload %s\n", assetLoaded);
    Memory *m = get_header(g);
    const char *pLastDot = strrchr(assetLoaded, '.');
    if (pLastDot == nullptr)
        return;
    
    size_t len = strlen(assetLoaded);
    printf("extension %s \n", pLastDot);
    if (strcmp(pLastDot, ".toml") == 0) {
        
        for (size_t i = 0; i < m->shaders->count; ++i) {
            glDeleteProgram(m->shaders->program_ids[i]);
            printf("Deleted shader program: %u\n", m->shaders->program_ids[i]);
        }
        
        reset_memory(m);
        
        ecs_load_level(g, assetLoaded);
        printf("reload scene\n");
    }
    
    if (strcmp(pLastDot, ".frag") == 0 || strcmp(pLastDot, ".vert") == 0) {
        printf("reload shader\n");
    }
}

static void glfw_error_callback(int error, const char *description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

EXPORT void init_engine(game *g) { init_engine_memory(g); }

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
    Memory *m = get_header(g);
    systems(g, m);
}

void EditTransform(float *cameraView, float *cameraProjection, float *matrix,
                   bool editTransformDecomposition) {
    
    ImGuiIO &io = ImGui::GetIO();
    
    static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
    static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    ImGuizmo::Manipulate(cameraView, cameraProjection, mCurrentGizmoOperation,
                         mCurrentGizmoMode, matrix, NULL, NULL);
}

EXPORT void draw_editor(game *g) {
    ImGui::SetCurrentContext(g->ctx);
    ImGui::SetAllocatorFunctions(g->alloc_func, g->free_func, g->user_data);
    
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
    ImGuizmo::BeginFrame();
    ImGui::Begin("Editor");
    
    ImVec2 viewportPos = ImVec2(0, 0);
    ImVec2 viewportSize = ImGui::GetIO().DisplaySize;
    ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x,
                      viewportSize.y);
    
    World *w = get_world(g);
    Memory *m = get_header(g);
    size_t count = w->entity_count;
    
    static size_t selected_entity = SIZE_MAX;
    
    if (ImGui::Button("Save World")) {
        const char *saveFilePath = "scene.toml";
        save_level(m, saveFilePath);
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
            get_component(m, selected_entity, &position);
            
            size_t camera_entity;
            if (!get_entity(m, CameraComponent, camera_entity)) {
                printf("couldn't find camera entity\n");
            }
            
            Position p;
            if (!get_component(m, camera_entity, &p)) {
                return;
            }
            Rotation r;
            if (!get_component(m, camera_entity, &r)) {
                return;
            }
            Camera c;
            if (!get_component(m, camera_entity, &c)) {
                printf("couldn't find camera entity\n");
            }
            if (selected_entity != camera_entity) {
                
                glm::vec3 cameraPosition = glm::vec3(p.x, p.y, p.z);
                glm::vec3 targetPosition = glm::vec3(1.0f, 0.0f, 0.0f);
                glm::vec3 upDirection = glm::vec3(0.0f, 1.0f, 0.0f);
                glm::quat orientation = glm::quat(r.w, r.x, r.y, r.z);
                glm::mat4 rotMat = glm::mat4_cast(orientation);
                glm::mat4 traslationMat =
                    glm::translate(glm::mat4(1.0f), -cameraPosition);
                
                glm::mat4 view = rotMat * traslationMat;
                
                float aspectRatio = 16.0f / 9.0f;
                
                glm::mat4 projection =
                    glm::perspective(glm::radians(70.0f), aspectRatio, 0.01f, 1000.0f);
                
                glm::mat4 modelMatrix = glm::translate(
                                                       glm::mat4(1.0f), glm::vec3(position.x, position.y, position.z));
                
                EditTransform(glm::value_ptr(view), glm::value_ptr(projection),
                              glm::value_ptr(modelMatrix), true);
                
                position.x = modelMatrix[3][0];
                position.y = modelMatrix[3][1];
                position.z = modelMatrix[3][2];
                set_component(m, selected_entity, position);
                
                if (ImGui::InputFloat3("Position", (float *)&position)) {
                    set_component(m, selected_entity, position);
                    modelMatrix = glm::translate(
                                                 glm::mat4(1.0f), glm::vec3(position.x, position.y, position.z));
                }
            } else {
                if (ImGui::InputFloat3("Camera Position", (float *)&position)) {
                    set_component(m, selected_entity, position);
                }
            }
        }
    }
    ImGui::End();
}
