#include "engine.h"
#include "memory.h"
#include <externals.h>
#include <game.h>
#include <stdio.h>
#include <toml.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "ecs.h"

EXPORT void load_level(game *g, const char* sceneFilePath){
  printf("load level at path %s\n", sceneFilePath);

  FILE *fp;
    char errbuf[200];

    fp = fopen(sceneFilePath, "r");
    if (!fp) {
        printf("Cannot open %s - %s\n", sceneFilePath, strerror(errno));
        return;
    }

    toml_table_t *level = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    printf("toml level loaded\n");

    for (int i = 0; ; i++) {
        const char *friendly_name = toml_key_in(level, i);
        if (!friendly_name) break;

        toml_table_t *attributes = toml_table_in(level, friendly_name);
        if (attributes) {
            printf("[%s]\n", friendly_name);

            for (int j = 0; ; j++) {
                const char *type_key = toml_key_in(attributes, j);
                if (!type_key) break;

                 switch (mapStringToSubkeyType(type_key)) {
                    case POSITION_TYPE: case SCALE_TYPE:  {
                        toml_table_t *nested_table = toml_table_in(attributes, type_key);
                        if (nested_table) {
                            toml_datum_t x = toml_double_in(nested_table, "x");
                            toml_datum_t y = toml_double_in(nested_table, "y");
                            toml_datum_t z = toml_double_in(nested_table, "z");

                            printf("  %s = { x = %.2f, y = %.2f, z = %.2f }\n", type_key, x.u.d, y.u.d, z.u.d);
                        }
                        break;
                    }
                    case ROTATION_TYPE:{
                        toml_table_t *nested_table = toml_table_in(attributes, type_key);
                        if (nested_table) {
                            toml_datum_t x = toml_double_in(nested_table, "pitch");
                            toml_datum_t y = toml_double_in(nested_table, "yaw");
                            toml_datum_t z = toml_double_in(nested_table, "roll");

                            printf("  %s = { x = %.2f, y = %.2f, z = %.2f }\n", type_key, x.u.d, y.u.d, z.u.d);
                        }
                        break;
                    }
                    case COLOR_TYPE: {
                        toml_table_t *nested_table = toml_table_in(attributes, type_key);
                        if (nested_table) {
                            toml_datum_t r = toml_double_in(nested_table, "r");
                            toml_datum_t g = toml_double_in(nested_table, "g");
                            toml_datum_t b = toml_double_in(nested_table, "b");

                            printf("  %s = { r = %.2f, g = %.2f, b = %.2f }\n", type_key, r.u.d, g.u.d, b.u.d);
                        }
                        break;
                    }
                    case MODEL_TYPE: {
                        break;
                    }
                    case MATERIAL_TYPE: {
                        break;
                    }
                    case TEXTURE_TYPE: {
                        break;
                    }
                    case FOV_TYPE: {
                        toml_datum_t fov_value = toml_double_in(attributes, type_key);
                        if (fov_value.ok) printf("  %s = %.2f\n", type_key, fov_value.u.d);
                        break;
                    }
                    case INTENSITY_TYPE: {
                        toml_datum_t intensity_value = toml_double_in(attributes, type_key);
                        if (intensity_value.ok) printf("  %s = %.2f\n", type_key, intensity_value.u.d);
                        break;
                    }
                    default:
                        printf("  %s = (unknown type)\n", type_key);
                }
            }
            printf("\n");
        }
    }

    toml_free(level);
}

EXPORT void init_engine(game *g)
{
  init_engine_memory(g);
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
    if(w->component_masks[selected_entity]){
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
