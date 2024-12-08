#pragma once

bool LoadGLTFMeshes(struct MemoryHeader *h, const char *meshFilePath,
					struct Model *outMesh);

unsigned int createShaderProgram(const char *vertexShaderSource,
								 const char *fragmentShaderSource);

void load_shaders(struct game *g);