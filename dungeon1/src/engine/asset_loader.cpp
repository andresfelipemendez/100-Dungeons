#include "asset_loader.h"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <filesystem>

uint32_t LoadSkinnedMesh() {
	return 0;
}

bool LoadGLTFMeshes(const char* meshFilePath) {

	constexpr auto gltfOptions =
        fastgltf::Options::LoadExternalBuffers |
        fastgltf::Options::LoadExternalImages |
		fastgltf::Options::GenerateMeshIndices;

	fastgltf::Parser parser;

	std::filesystem::path path = "assets/models/static/Knight.glb";
	auto data = fastgltf::GltfDataBuffer::FromPath(path);
	if (data.error() != fastgltf::Error::None) {
		return false;
	}

	auto asset = parser.loadGltf(data.get(), path.parent_path(), gltfOptions);
	if (auto error = asset.error(); error != fastgltf::Error::None) {
		return false;
	}


	for (auto& mesh : asset.get().meshes) {
	 	Mesh outMesh = {};
		outMesh.primitives.resize(mesh.primitives.size());
	    for (auto it = mesh.primitives.begin(); it != mesh.primitives.end(); ++it) {
	    	GLuint vao = GL_NONE;
	        glCreateVertexArrays(1, &vao);

			std::size_t baseColorTexcoordIndex = 0;

			auto* positionIt = it->findAttribute("POSITION");
			auto index = std::distance(mesh.primitives.begin(), it);
			auto& primitive = outMesh.primitives[index];
			primitive.primitiveType = fastgltf::to_underlying(it->type);
		}
	}
		

	uint32_t id = LoadSkinnedMesh();
	return true;
}
