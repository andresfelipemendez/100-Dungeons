#pragma once

bool LoadGLTFMeshes(struct MemoryHeader* h, const char* meshFilePath);

unsigned int createShaderProgram(const char* vertexShaderSource,const  char* fragmentShaderSource);