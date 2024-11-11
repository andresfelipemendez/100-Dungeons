#pragma once

bool LoadGLTFMeshes(struct MemoryHeader *h, const char *meshFilePath,
					struct StaticMesh *outMesh);

unsigned int createShaderProgram(const char *vertexShaderSource,
								 const char *fragmentShaderSource);