#include "asset_loader.h"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>

bool LoadGLTFMeshes(const char* meshFilePath){
	fastgltf::Parser parser;

	std::filesystem::path path = "assets/models/static/Knight.glb";
	auto data = fastgltf::GltfDataBuffer::FromPath(path);
	if (data.error() != fastgltf::Error::None) {
		// The file couldn't be loaded, or the buffer could not be allocated.
		return false;
	}

	auto asset = parser.loadGltf(data.get(), path.parent_path(), fastgltf::Options::None);
	if (auto error = asset.error(); error != fastgltf::Error::None) {
		// Some error occurred while reading the buffer, parsing the JSON, or validating the data.
		return false;
	}

	fastgltf::validate(asset.get());

	printf("Loaded GLTF Model %s\n", path.string().c_str());
	return true;
}