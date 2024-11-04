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

#define GLAD_GLAPI_EXPORT 
#include <glad.h>
#include <GLFW/glfw3.h>

#include <vector>

unsigned int VAO, VBO, shaderProgram;

EXPORT void load_shader(game *g) {

}

EXPORT void load_level(game *g, const char *sceneFilePath)
{
	printf("load level at path %s\n", sceneFilePath);
	ecs_load_level(g, sceneFilePath);
}

EXPORT void init_engine(game *g)
{
	init_engine_memory(g);
}

static std::vector<Mesh> meshes;


EXPORT void load_meshes(game *g) {
	meshes = LoadGLTFMeshes("");

    float vertices[] = {
        0.0f,  0.5f, 0.0f,  
        -0.5f, -0.5f, 0.0f, 
        0.5f, -0.5f, 0.0f   
    };

    glCreateVertexArrays(1, &VAO);
    glCreateBuffers(1, &VBO);

    glNamedBufferData(VBO, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexArrayVertexBuffer(VAO, 0, VBO, 0, 3 * sizeof(float));
    glEnableVertexArrayAttrib(VAO, 0);
    glVertexArrayAttribFormat(VAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(VAO, 0, 0);

    const char *vertexShaderSource = "#version 450 core\n"
                                     "layout (location = 0) in vec3 aPos;\n"
                                     "void main()\n"
                                     "{\n"
                                     "   gl_Position = vec4(aPos, 1.0);\n"
                                     "}\0";

    const char *fragmentShaderSource = "#version 450 core\n"
                                       "out vec4 FragColor;\n"
                                       "void main()\n"
                                       "{\n"
                                       "   FragColor = vec4(1.0, 1.0, 0.2, 1.0);\n"
                                       "}\0";
    
    shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    printf("this should be done only once when the engine begins\n");
}

static void glfw_error_callback(int error, const char *description) {
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

EXPORT void begin_frame(game *g)
{
	glfwSetErrorCallback(glfw_error_callback);
	glfwMakeContextCurrent(g->window);
	if (!glfwGetCurrentContext()) {
	    fprintf(stderr, "No current OpenGL context detected in DLL\n");
	    return;
	}

	if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
	   	printf("Failed to initialize GLAD in DLL\n");
	    return;
	}

}

EXPORT void draw_opengl(game *g) {
	glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
	// for( const auto& mesh : meshes) {
	// 	 glBindBuffer(GL_DRAW_INDIRECT_BUFFER, mesh.drawsBuffer);
	// 	 glUniformMatrix4fv(viewer->modelMatrixUniform, 1, GL_FALSE, &matrix[0][0]);
	// }
}

EXPORT void hotreloadable_imgui_draw(game *g)
{
	ImGui::SetCurrentContext(g->ctx);
	ImGui::SetAllocatorFunctions(g->alloc_func, g->free_func, g->user_data);
	ImGui::Begin("Hello! world from reloadable dll!");
	
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
