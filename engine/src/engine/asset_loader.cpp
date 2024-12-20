#include "memory.h"
#include <cstring>
#define WIN32_LEAN_AND_MEAN
#include "asset_loader.h"

#include "ecs.h"
#include <cstdio>
#include <cstdlib>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <glad.h>

#include <printLog.h>

#include <Windows.h>
#include <minwinbase.h>
#include <winnt.h>

bool LoadGLTFMeshes(Memory *m, const char *meshFilePath, Model *outMesh) {
  return true;
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
