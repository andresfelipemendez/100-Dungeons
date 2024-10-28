#pragma once

struct MeshID {
    unsigned int VAO;
    unsigned int vertexCount;
};

MeshID loadModel();