#include "externals.h"
#include <game.h>
#include <stdio.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <corecrt_malloc.h>

#include <glad.h>

#include <GLFW/glfw3.h>
#include <printLog.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <components.h>
#include <ecs.h>
#include <memory.h>


#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

static void glfw_error_callback(int error, const char *description) {
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

EXPORT int init_externals(game *g) {
  glfwSetErrorCallback(glfw_error_callback);

  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    return -1;
  }

  const char *glsl_version = "#version 450";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  g->window = glfwCreateWindow(1920, 1080, "Hello World", NULL, NULL);
  if (!g->window) {
    fprintf(stderr, "Failed to create GLFW window\n");
    glfwTerminate();
    return -1;
  }

  int monitorCount;
  GLFWmonitor **monitors = glfwGetMonitors(&monitorCount);
  GLFWmonitor *monitor;
  if (monitorCount >= 3) {
    monitor = monitors[1];
  } else {
    monitor = monitors[0];
  }
  int monitorX, monitorY;
  glfwGetMonitorPos(monitor, &monitorX, &monitorY);

  const GLFWvidmode *vidMode = glfwGetVideoMode(monitor);
  if (vidMode == NULL) {
    fprintf(stderr, "Failed to get video mode for monitor #4\n");
    glfwDestroyWindow(g->window);
    glfwTerminate();
    return -1;
  }

  printf("monitor widht %i, height %i\n", vidMode->width, vidMode->height);
  if (vidMode->width == 2560 && vidMode->height == 1600) {
    int windowWidth = 1920;
    int windowHeight = 1080;
    int xpos = monitorX /*+ (vidMode->width - windowWidth)*/;
    int ypos = monitorY + 30;
    glfwSetWindowPos(g->window, xpos, ypos);
  }

  glfwMakeContextCurrent(g->window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGuiContext *ctx = ImGui::CreateContext();
  if (!ctx) {
    fprintf(stderr, "Failed to create ImGui context\n");
    glfwDestroyWindow(g->window);
    glfwTerminate();
    return -1;
  }

  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

  ImGui::StyleColorsDark();
  ImGui::SetCurrentContext(ctx);

  if (!ImGui_ImplGlfw_InitForOpenGL(g->window, true)) {
    fprintf(stderr, "Failed to initialize ImGui_ImplGlfw\n");
    ImGui::DestroyContext(ctx);
    glfwDestroyWindow(g->window);
    glfwTerminate();
    return -1;
  }

  if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
    fprintf(stderr, "Failed to initialize ImGui_ImplOpenGL3\n");
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(ctx);
    glfwDestroyWindow(g->window);
    glfwTerminate();
    return -1;
  }

  g->loader = (GLADloadproc)glfwGetProcAddress;
  if (!gladLoadGLLoader((GLADloadproc)g->loader)) {
    printf("Failed to initialize GLAD in DLL\n");
    return -1;
  }
  g->play = true;
  g->ctx = ctx;
  ImGui::GetAllocatorFunctions(&g->alloc_func, &g->free_func, &g->user_data);

  return 1;
}

// EXPORT void free_mesh()
char *read_file(const char *filepath) {
  FILE *file = NULL;
  errno_t err = fopen_s(&file, filepath, "rb");
  if (err != 0 || file == NULL) {
    printf("Failed to open file: %s\n", filepath);
  }
  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *content = (char *)malloc(length + 1);
  if (!content) {
    printf("Failed to allocate memory for the shader file content\n");
    fclose(file);
    return nullptr;
  }

  fread(content, 1, length, file);
  content[length] = '\0';
  fclose(file);
  return content;
}


unsigned int createShaderProgram(const char *vertexSource,
                                 const char *fragmentShaderSource) {
  unsigned int vertexShader, fragmentShader;
  int success;
  char infoLog[512];

  // Vertex Shader
  vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vertexSource, NULL);
  glCompileShader(vertexShader);
  glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
    fprintf(stderr, "Vertex Shader Compilation Error:\n%s\n", infoLog);
  }

  // Fragment Shader
  fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
  glCompileShader(fragmentShader);
  glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
    fprintf(stderr, "Fragment Shader Compilation Error:\n%s\n", infoLog);
  }

  // Shader Program
  unsigned int program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glLinkProgram(program);
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(program, 512, NULL, infoLog);
    fprintf(stderr, "Shader Program Linking Error:\n%s\n", infoLog);
  }

  // Clean up shaders as they are no longer needed after linking
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return program;
}


bool add_shader(Memory *m, char *name, GLuint programID) {
  size_t name_length = strlen(name);
  if (name_length > ENTITY_NAME_LENGTH) {
    printf("shader name should be less than %i, name: %s \n",
           ENTITY_NAME_LENGTH, name);
  }

  for (size_t i = 0; i < name_length; ++i) {
    name[i] = tolower((unsigned char)name[i]);
  }

  errno_t err = strncpy_s(m->shaders->shader_names[m->shaders->count],
                          ENTITY_NAME_LENGTH, name, name_length);
  if (err != 0) {
    printf("error copying shader name %s\n", name);
    return false;
  }

  m->shaders->shader_names[m->shaders->count][name_length] = '\0';
  m->shaders->program_ids[m->shaders->count] = programID;
  m->shaders->count++;

  return true;
}

EXPORT void load_shaders(game *g) {
// I need to get the shader ID from the map of string to ID
  char cwd[MAX_PATH];
  GetCurrentDirectoryA(MAX_PATH, cwd);

  char shaderDir[MAX_PATH];
  snprintf(shaderDir, sizeof(shaderDir), "%s\\assets\\shaders", cwd);

  char searchPath[MAX_PATH];
  snprintf(searchPath, sizeof(searchPath), "%s\\*.vert", shaderDir);

  WIN32_FIND_DATA findFileData;
  HANDLE hFind = FindFirstFile(searchPath, &findFileData);

  if (hFind == INVALID_HANDLE_VALUE) {
    printf("Failed to open shader directory: %s\n", shaderDir);
    return;
  }

  

  Memory *m = (Memory *)((char *)g->buffer + g->buffer_size - sizeof(Memory));
  do {
    const char *vertFileName = findFileData.cFileName;

    char shaderName[MAX_PATH];
    strncpy_s(shaderName, sizeof(shaderName), vertFileName,
              strlen(vertFileName) - 5);

    char vertPath[MAX_PATH];
    snprintf(vertPath, sizeof(vertPath), "%s\\%s", shaderDir, vertFileName);
    char fragPath[MAX_PATH];
    strncpy_s(fragPath, sizeof(fragPath), vertPath, strlen(vertPath) - 5);
    strcat_s(fragPath, sizeof(fragPath), ".frag");

    char *vertexSource = read_file(vertPath);
    char *fragmentSource = read_file(fragPath);

    if (vertexSource && fragmentSource) {
      GLuint shaderProgram = createShaderProgram(vertexSource, fragmentSource);

      free(vertexSource);
      free(fragmentSource);
      printf("shader %s program id is: %i\n", shaderName, shaderProgram);
      add_shader(m, shaderName, shaderProgram);
    }

  } while (FindNextFile(hFind, &findFileData) != 0);

  FindClose(hFind);

}

EXPORT void load_mesh(game *g, const char *meshFilePath) {
  Memory *m = (Memory *)((char *)g->buffer + g->buffer_size - sizeof(Memory));

  printf("load mesh from external %s \n", meshFilePath);

  constexpr auto gltfOptions = fastgltf::Options::LoadExternalBuffers |
                               fastgltf::Options::LoadExternalImages |
                               fastgltf::Options::GenerateMeshIndices;

  fastgltf::Parser parser;

  std::filesystem::path fullMeshPath =
      std::filesystem::current_path() / meshFilePath;
  fullMeshPath.make_preferred();

  auto data = fastgltf::GltfDataBuffer::FromPath(fullMeshPath);
  if (data.error() != fastgltf::Error::None) {
    print_log(COLOR_RED, "error loading model");
    return;
  }

  auto asset =
      parser.loadGltf(data.get(), fullMeshPath.parent_path(), gltfOptions);
  if (auto error = asset.error(); error != fastgltf::Error::None) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer),
             "Error loading GLTF data from file: %s (Error Code: %s)",
             fullMeshPath.string().c_str(),
             fastgltf::getErrorMessage(error).data());

    print_log(COLOR_RED, buffer);
    return;
  }

  Model *outMesh =
      &m->components->pModels->components[m->components->pModels->count];
  for (auto &mesh : asset.get().meshes) {
    for (auto it = mesh.primitives.begin(); it != mesh.primitives.end(); ++it) {

      outMesh->submesh_count++;

      if (it->type != fastgltf::PrimitiveType::Triangles) {
        print_log(COLOR_RED, "submesh type it's not GL_TRIANGLES\n");
        return;
      }

      GLuint vao = GL_NONE;
      glCreateVertexArrays(1, &vao);
      size_t baseColorTexcoordIndex = 0;

      auto *positionIt = it->findAttribute("POSITION");
      auto index = std::distance(mesh.primitives.begin(), it);

      SubMesh *subMesh = &outMesh->submeshes[index];
      subMesh->vertexArray = vao;
      auto &positionAccessor = asset.get().accessors[positionIt->accessorIndex];
      if (!positionAccessor.bufferViewIndex.has_value())
        continue;

      glCreateBuffers(1, &subMesh->vertexBuffer);

      glNamedBufferData(subMesh->vertexBuffer,
                        positionAccessor.count * sizeof(Vertex), nullptr,
                        GL_STATIC_DRAW);

      auto *vertices = static_cast<Vertex *>(
          glMapNamedBuffer(subMesh->vertexBuffer, GL_WRITE_ONLY));

      // Load Position
      fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
          asset.get(), positionAccessor,
          [&](fastgltf::math::fvec3 pos, std::size_t idx) {
            vertices[idx].position = Vec3{pos.x(), pos.y(), pos.z()};
          });

      // Load Normals
      auto *normalIt = it->findAttribute("NORMAL");
      if (normalIt != it->attributes.end()) {
        auto &normalAccessor = asset.get().accessors[normalIt->accessorIndex];
        if (normalAccessor.bufferViewIndex.has_value()) {
          fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
              asset.get(), normalAccessor,
              [&](fastgltf::math::fvec3 norm, std::size_t idx) {
                vertices[idx].normal = Vec3{norm.x(), norm.y(), norm.z()};
              });
        }
      }

      // Load Texture Coordinates
      auto texcoordAttribute =
          std::string("TEXCOORD_") + std::to_string(baseColorTexcoordIndex);

      if (const auto *texcoord = it->findAttribute(texcoordAttribute);
          texcoord != it->attributes.end()) {
        auto &texCoordAccessor = asset.get().accessors[texcoord->accessorIndex];
        if (texCoordAccessor.bufferViewIndex.has_value()) {
          fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
              asset.get(), texCoordAccessor,
              [&](fastgltf::math::fvec2 uv, std::size_t idx) {
                vertices[idx].uv = Vec2{uv.x(), uv.y()};
              });
        }
      }

      glUnmapNamedBuffer(subMesh->vertexBuffer);

      // Bind Position Attribute to VAO
      glBindVertexArray(subMesh->vertexArray);
      glBindBuffer(GL_ARRAY_BUFFER, subMesh->vertexBuffer);

      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                            (void *)offsetof(Vertex, position));

      // Bind Normal Attribute to VAO
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                            (void *)offsetof(Vertex, normal));

      // Bind Texture Coordinate Attribute to VAO
      glEnableVertexAttribArray(2);
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                            (void *)offsetof(Vertex, uv));

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subMesh->indexBuffer);
      glBindVertexArray(0);

      auto &draw = subMesh->draw;
      draw.instanceCount = 1;
      draw.baseInstance = 0;
      draw.baseVertex = 0;
      draw.firstIndex = 0;

      auto &indexAccessor = asset.get().accessors[it->indicesAccessor.value()];
      if (!indexAccessor.bufferViewIndex.has_value())
        return;
      draw.count = static_cast<uint32_t>(indexAccessor.count);
      glCreateBuffers(1, &subMesh->indexBuffer);

      if (indexAccessor.componentType ==
              fastgltf::ComponentType::UnsignedByte ||
          indexAccessor.componentType ==
              fastgltf::ComponentType::UnsignedShort) {
        subMesh->indexType = GL_UNSIGNED_SHORT;
        glNamedBufferData(subMesh->indexBuffer,
                          static_cast<GLsizeiptr>(indexAccessor.count *
                                                  sizeof(std::uint16_t)),
                          nullptr, GL_STATIC_DRAW);
        auto *indices = static_cast<std::uint16_t *>(
            glMapNamedBuffer(subMesh->indexBuffer, GL_WRITE_ONLY));
        fastgltf::copyFromAccessor<std::uint16_t>(asset.get(), indexAccessor,
                                                  indices);
        glUnmapNamedBuffer(subMesh->indexBuffer);
      } else {
        subMesh->indexType = GL_UNSIGNED_INT;
        glNamedBufferData(subMesh->indexBuffer,
                          static_cast<GLsizeiptr>(indexAccessor.count *
                                                  sizeof(std::uint32_t)),
                          nullptr, GL_STATIC_DRAW);
        auto *indices = static_cast<std::uint32_t *>(
            glMapNamedBuffer(subMesh->indexBuffer, GL_WRITE_ONLY));
        fastgltf::copyFromAccessor<std::uint32_t>(asset.get(), indexAccessor,
                                                  indices);
        glUnmapNamedBuffer(subMesh->indexBuffer);
      }

      glVertexArrayElementBuffer(vao, subMesh->indexBuffer);
      glCreateBuffers(1, &outMesh->drawsBuffer);

      glNamedBufferData(
          outMesh->drawsBuffer,
          static_cast<GLsizeiptr>(outMesh->submesh_count * sizeof(SubMesh)),
          outMesh->submeshes, GL_STATIC_DRAW);
    }
  }
}

EXPORT void update_externals(game *g) {
  glfwMakeContextCurrent(g->window);
  glfwPollEvents();
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  g->draw_editor(g);

  ImGui::SetCurrentContext(g->ctx);
  ImGui::SetAllocatorFunctions(g->alloc_func, g->free_func, g->user_data);

  ImGui::Render();
  int display_w, display_h;
  glfwGetFramebufferSize(g->window, &display_w, &display_h);
  glViewport(0, 0, display_w, display_h);

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  glfwSwapBuffers(g->window);

  g->play = !glfwWindowShouldClose(g->window);
}

EXPORT void end_externals(game *g) {
  g->play.store(false);
  glfwMakeContextCurrent(g->window);

  if (g->ctx) {
    ImGui::SetCurrentContext(g->ctx);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext(g->ctx);
    g->ctx = nullptr;
  }

  if (g->window) {
    glfwDestroyWindow(g->window);
    g->window = nullptr;
  }

  glfwTerminate();

  print_log(COLOR_YELLOW, "Externals have been successfully shut down");
}

EXPORT ImGuiContext *GetImguiContext() { return ImGui::GetCurrentContext(); }
