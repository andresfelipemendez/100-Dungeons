#pragma once

#include <vector>
#include <fastgltf/types.hpp>
#include <glad.h>

struct Viewer {
    fastgltf::Asset asset;
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

bool LoadGLTFMeshes(const char* meshFilePath);

//uint32_t LoadSkinnedMesh();