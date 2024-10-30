#include "engine.h"
#include "memory.h"
#include <externals.h>
#include <game.h>
#include <stdio.h>

//#define IMGUI_USER_CONFIG "myimguiconfig.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "ecs.h"

EXPORT void init_engine(game *g) {
  init_engine_memory(g);
}

EXPORT void hotreloadable_imgui_draw(game *g) {
  ImGui::SetCurrentContext(g->ctx);
  ImGui::SetAllocatorFunctions(g->alloc_func, g->free_func, g->user_data);
  ImGui::Begin("Hello! world from reloadable dll!");
  ImGui::Text("This is some useful text.");  

  World* w = get_world(g);
  size_t count = w->entity_count;

  static size_t selected_entity = SIZE_MAX; 

  if(ImGui::Button("add entity")) {
    selected_entity = create_entity(g);
  }

  ImGui::Text("Entity List:");
  for (size_t i = 0; i < count; ++i) {
    if(ImGui::Selectable(w->entity_names[i], selected_entity == i)){
      selected_entity = i;
    }
  }

  ImGui::Separator();

  if (selected_entity != SIZE_MAX) {
    ImGui::Text("Inspecting Entity: %s", w->entity_names[selected_entity]);

    if (w->component_masks[selected_entity] & COMPONENT_POSITION) {
      ImGui::Text("Position:");
      //  ImGui::InputFloat3("Position", (float*)&w->positions[selected_entity]);
    }
  }

  ImGui::End();
}
