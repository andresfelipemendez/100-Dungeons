#include "engine.h"
#include "memory.h"
#include <externals.h>
#include <game.h>
#include <stdio.h>

#include "asset_loader.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "ecs.h"

EXPORT void load_level(game *g, const char *sceneFilePath)
{
	printf("load level at path %s\n", sceneFilePath);
	ecs_load_level(g, sceneFilePath);
	
}

EXPORT void init_engine(game *g)
{
	init_engine_memory(g);
	LoadGLTFMeshes("");
}

EXPORT void hotreloadable_imgui_draw(game *g)
{
	ImGui::SetCurrentContext(g->ctx);
	ImGui::SetAllocatorFunctions(g->alloc_func, g->free_func, g->user_data);
	ImGui::Begin("Hello! world from reloadable dll!");
	ImGui::Text("This is some useful text.");

	World *w = get_world(g);
	MemoryHeader *h = get_header(g);
	size_t count = w->entity_count;

	static size_t selected_entity = SIZE_MAX;

    if (ImGui::Button("Save World"))
    {
        const char* saveFilePath = "ecs_world_save.toml";
        save_level(h, saveFilePath);
        ImGui::Text("World saved to %s", saveFilePath);
    }
    
	if (ImGui::Button("add entity"))
	{
		selected_entity = create_entity(w);
	}

	ImGui::Text("Entity List:");
	for (size_t i = 0; i < count; ++i)
	{
		if (ImGui::Selectable(w->entity_names[i], selected_entity == i))
		{
			selected_entity = i;
		}
	}

	ImGui::Separator();

	if (selected_entity != SIZE_MAX)
	{
		ImGui::Text("Inspecting Entity: %s", w->entity_names[selected_entity]);

		static int selected_component = -1;

		if (ImGui::BeginCombo("Add Component", "Select Component"))
		{
			for (int i = 0; i < component_count; ++i)
			{
				if (ImGui::Selectable(component_names[i], selected_component == i))
				{
					selected_component = i;
					add_component(h, selected_entity, (1 << i));
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Separator();
		if (w->component_masks[selected_entity])
		{
			ImGui::Text("Components:");
		}
		if (w->component_masks[selected_entity] & COMPONENT_POSITION)
		{
			Vec3 position;

			get_component_value(h, selected_entity, COMPONENT_POSITION, &position);
			if (ImGui::InputFloat3("Position", (float *)&position))
			{
				set_component_value(h, selected_entity, COMPONENT_POSITION, position);
			}
		}
	}

	ImGui::End();
}
