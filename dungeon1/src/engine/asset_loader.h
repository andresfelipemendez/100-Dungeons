#pragma once

#include <vector>
#include <fastgltf/types.hpp>
#include <glad.h>

struct Viewer {
    fastgltf::Asset asset;

   /* std::vector<GLuint> bufferAllocations;
    std::vector<Mesh> meshes;
    std::vector<Texture> textures;
    std::vector<fastgltf::math::fmat4x4> cameras;

    std::vector<MaterialUniforms> materials;
    std::vector<GLuint> materialBuffers;

    GLint uvOffsetUniform = GL_NONE;
    GLint uvScaleUniform = GL_NONE;
    GLint uvRotationUniform = GL_NONE;

    fastgltf::math::ivec2 windowDimensions = fastgltf::math::ivec2(0);
    fastgltf::math::fmat4x4 viewMatrix = fastgltf::math::fmat4x4(1.0f);
    fastgltf::math::fmat4x4 projectionMatrix = fastgltf::math::fmat4x4(1.0f);
    GLint viewProjectionMatrixUniform = GL_NONE;
    GLint modelMatrixUniform = GL_NONE;

    float lastFrame = 0.0f;
    float deltaTime = 0.0f;
    fastgltf::math::fvec3 accelerationVector = fastgltf::math::fvec3(0.0f);
    fastgltf::math::fvec3 velocity = fastgltf::math::fvec3(0.0f);
    fastgltf::math::fvec3 position = fastgltf::math::fvec3(0.0f, 0.0f, 0.0f);

    fastgltf::math::dvec2 lastCursorPosition = fastgltf::math::dvec2(0.0f);
    fastgltf::math::fvec3 direction = fastgltf::math::fvec3(0.0f, 0.0f, -1.0f);
    float yaw = -90.0f;
    float pitch = 0.0f;
    bool firstMouse = true;

    std::size_t sceneIndex = 0;
    std::size_t materialVariant = 0;
    fastgltf::Optional<std::size_t> cameraIndex = std::nullopt;*/
};
struct IndirectDrawCommand {
    std::uint32_t count;
    std::uint32_t instanceCount;
    std::uint32_t firstIndex;
    std::int32_t baseVertex;
    std::uint32_t baseInstance;
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