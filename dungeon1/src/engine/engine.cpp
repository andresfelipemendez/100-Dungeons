#include "engine.h"
#include <externals.h>
#include <game.h>
#include <stdio.h>


#define IMGUI_USER_CONFIG "myimguiconfig.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "ecs.h"

void MySaveFunction(game *g, const char* entity_name) {
  world* w  = (world*) g->world;
  w->entitiesCount  ++;
}

static char new_name[128] = ""; 

EXPORT void hotreloadable_imgui_draw(game *g) {
  world* w  = (world*) g->world;
  ImGui::SetCurrentContext(g->ctx);
  ImGui::SetAllocatorFunctions(g->alloc_func, g->free_func, g->user_data);
  ImGui::Begin("Hello! world from reloadable dll!");
  ImGui::Text("This is some useful text.");
  ImGui::InputInt("entitiesCount", &w->entitiesCount);
  ImGui::Separator();

  ImGui::InputText("New Entity Name", new_name, IM_ARRAYSIZE(new_name));
  if (ImGui::Button("Add New Entity")) {
        MySaveFunction(g, new_name);  // Create a new entity with the specified name
        new_name[0] = '\0';           // Clear the name input after creation
    }
  ImGui::End();
}
