#include "engine.h"
#include <externals.h>
#include <game.h>
#include <stdio.h>


#define IMGUI_USER_CONFIG "myimguiconfig.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <ecs.h>

void MySaveFunction(game *g, const char* entity_name) {
  World* w  = (World*) g->world;
  w->entity_count  ++;
}

static char new_name[128] = ""; 

EXPORT void hotreloadable_imgui_draw(game *g) {
  World* w  = (World*) g->world;
  ImGui::SetCurrentContext(g->ctx);
  ImGui::SetAllocatorFunctions(g->alloc_func, g->free_func, g->user_data);
  ImGui::Begin("Hello! world from reloadable dll!");
  ImGui::Text("This is some useful text.");
  int temp_entity_count = static_cast<int>(w->entity_count);
  if(ImGui::InputInt("entitiesCount", &temp_entity_count)){
     w->entity_count = static_cast<size_t>(temp_entity_count);
  }

  ImGui::Separator();

  ImGui::InputText("New Entity Name", new_name, IM_ARRAYSIZE(new_name));
  if (ImGui::Button("Add New Entity")) {
        MySaveFunction(g, new_name);  
        new_name[0] = '\0';           
    }
  ImGui::End();
}
