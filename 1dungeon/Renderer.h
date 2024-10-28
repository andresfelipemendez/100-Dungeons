#pragma once
#include "entt/entt.hpp"
#include "Components.h"
#include <vector>

bool InitializeRenderer(struct GameData* gd);
uint32_t CreateSpriteVerts();
uint32_t LoadMesh(std::vector<float> vertices, std::vector<unsigned int> indices);
uint32_t UploadMeshToGPU(const std::vector<uint32_t>& indices, const std::vector<float>& vertices,
    size_t stride);
uint32_t LoadSkinnedMesh(const std::vector<SkinnedVertex>& vertices, const std::vector<unsigned int>& indices);
Texture LoadTexture(const std::string& fileName);
Shader LoadShader(const std::string& vertexShader, const std::string& fragmentShader);
void BeginDrawOpaque();
void BeginDrawTransparent();
void DrawMesh(size_t numVertices);
void EndDraw(GameData* gd);
void SetShaderActive(unsigned int shaderProgram);
void SetVerticesActive(unsigned int vertexID);
void SetTextureActive(unsigned int textureID);
void SetMeshActive(unsigned int vertexID);
void SetMatrixUniform(unsigned int shaderProgram, const char* uniformName, const class Matrix4* matrix);

void SetVectorUniform(unsigned int shaderProgram, const char* name, const Vector3& vector);