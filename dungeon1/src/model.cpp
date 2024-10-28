#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#include "tiny_gltf.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include "model.h"

using namespace tinygltf;

MeshID loadModel() {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, "assets/KayKit_DungeonRemastered_1.1_FREE/Assets/gltf/floor_tile_large.gltf");

    if (!warn.empty()) {
        fprintf(stderr, "Warn: %s", warn.c_str());
    }

    if (!err.empty()) {
        fprintf(stderr, "Err: %s", err.c_str());
    }

    if (!ret) {
        fprintf(stderr, "Failed to load glTF model\n");
        return {0, 0};
    }

    const tinygltf::Mesh &gltfMesh = model.meshes[0];
    const tinygltf::Accessor &posAccessor = model.accessors[gltfMesh.primitives[0].attributes.find("POSITION")->second];
    const tinygltf::BufferView &posView = model.bufferViews[posAccessor.bufferView];
    const tinygltf::Buffer &posBuffer = model.buffers[posView.buffer];

    unsigned int VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    unsigned int VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, posView.byteLength, reinterpret_cast<const void*>(&posBuffer.data[posView.byteOffset]), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    unsigned int vertexCount = posAccessor.count;
    // Set the VAO and vertex count to use later in rendering
    return {VAO, vertexCount};
}
