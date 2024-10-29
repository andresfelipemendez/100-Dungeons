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

EXPORT void init_engine(game *g) {
  printf("init_engine\n");

  MemoryHeader* header = (MemoryHeader*)g->world;

  // size_t total_entities = header->world.entity_count;
  // size_t offset = sizeof(MemoryHeader);

  // header->world.entity_ids = (size_t*)((char*)header + offset);
  // offset += sizeof(size_t) * total_entities;

  // header->world.component_masks = (uint32_t*)((char*)header + offset);
  // offset += sizeof(uint32_t) * total_entities; 

  // header->transforms.count = total_entities;
  // header->transforms.offset = offset;
  // offset += sizeof(size_t) * total_entities + sizeof(Vec3) * total_entities;

  // header->rotations.count = total_entities;
  // header->rotations.offset = offset;
  // offset += sizeof(size_t) * total_entities + sizeof(Vec3) * total_entities;

  // header->models.count = total_entities;
  // header->models.offset = offset;
  // offset += sizeof(size_t) * total_entities + sizeof(Vec3) * total_entities;

  // header->shaders.count = total_entities;
  // header->shaders.offset = offset;
  // offset += sizeof(size_t) * total_entities + sizeof(unsigned int) * total_entities;

  // header->textures.count = total_entities;
  // header->textures.offset = offset;
  // offset += sizeof(size_t) * total_entities + sizeof(Texture) * total_entities;

}

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

 
  ImGui::End();
}
