#pragma once

#include <vector>
#include <fastgltf/types.hpp>
#include <glad.h>

#include <vector>
struct Viewer {
    fastgltf::Asset asset;

    GLint modelMatrixUniform = GL_NONE;
};

struct IndirectDrawCommand {
    std::uint32_t count;
    std::uint32_t instanceCount;
    std::uint32_t firstIndex;
    std::int32_t baseVertex;
    std::uint32_t baseInstance;
};

struct Vertex {
	fastgltf::math::fvec3 position;
	fastgltf::math::fvec2 uv;
};

struct Primitive {
    IndirectDrawCommand draw;
    GLenum primitiveType;
    GLenum indexType;
    GLuint vertexArray;

    GLuint vertexBuffer;
    GLuint indexBuffer;

    std::size_t materialUniformsIndex;
    GLuint albedoTexture;
};

struct Mesh {
    GLuint drawsBuffer;
    std::vector<Primitive> primitives;
};

std::vector<Mesh> LoadGLTFMeshes(const char* meshFilePath);

//uint32_t LoadSkinnedMesh();

unsigned int createShaderProgram(const char* vertexShaderSource,const  char* fragmentShaderSource);