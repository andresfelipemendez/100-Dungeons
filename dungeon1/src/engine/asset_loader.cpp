#include "asset_loader.h"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <filesystem>

#include <vector>

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

	std::vector<Mesh> meshes;

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
			primitive.vertexArray = vao;
			// ?
			primitive.materialUniformsIndex = 0;

			auto& positionAccessor = asset.get().accessors[positionIt->accessorIndex];
			if (!positionAccessor.bufferViewIndex.has_value())
				continue;

			glCreateBuffers(1, &primitive.vertexBuffer);
			glNamedBufferData(
				primitive.vertexBuffer, 
				positionAccessor.count * sizeof(Vertex), 
				nullptr, 
				GL_STATIC_DRAW
				);
			auto* vertices = static_cast<Vertex*>(glMapNamedBuffer(primitive.vertexBuffer, GL_WRITE_ONLY));
			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
				asset.get(), positionAccessor, [&](fastgltf::math::fvec3 pos, std::size_t idx) {
					vertices[idx].position = fastgltf::math::fvec3(pos.x(), pos.y(), pos.z());
					vertices[idx].uv = fastgltf::math::fvec2();
				}
				);
			glUnmapNamedBuffer(primitive.vertexBuffer);

			glEnableVertexArrayAttrib(vao, 0);
			glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
			glVertexArrayAttribBinding(vao, 0, 0);
			glVertexArrayVertexBuffer(vao, 0, primitive.vertexBuffer, 0, sizeof(Vertex));

			auto texcoordAttribute = std::string("TEXCOORD_") + std::to_string(baseColorTexcoordIndex);
			if (const auto* texcoord = it->findAttribute(texcoordAttribute); texcoord != it->attributes.end()) {
				auto& texCoordAccessor = asset.get().accessors[texcoord->accessorIndex];
				if (!texCoordAccessor.bufferViewIndex.has_value())
					continue;
				auto* vertices = static_cast<Vertex*>(glMapNamedBuffer(primitive.vertexBuffer, GL_WRITE_ONLY));
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
					asset.get(), texCoordAccessor, [&](fastgltf::math::fvec2 uv, std::size_t idx) {
						vertices[idx].uv = fastgltf::math::fvec2(uv.x(), uv.y());
					}
					);
				glUnmapNamedBuffer(primitive.vertexBuffer);
				glEnableVertexArrayAttrib(vao,1);
				glVertexArrayAttribFormat(vao,1,2,GL_FLOAT,GL_FALSE,0);
				glVertexArrayAttribBinding(vao,1,1);
				glVertexArrayVertexBuffer(vao,1,primitive.vertexBuffer, offsetof(Vertex, uv), sizeof(Vertex));
			}

			auto& draw = primitive.draw;
			draw.instanceCount = 1;
			draw.baseInstance = 0;
			draw.baseVertex = 0;
			draw.firstIndex = 0;

			auto& indexAccessor = asset.get().accessors[it->indicesAccessor.value()];
			if(!indexAccessor.bufferViewIndex.has_value())
				return false;
			draw.count = static_cast<uint32_t>(indexAccessor.count);
			glCreateBuffers(1,&primitive.indexBuffer);

			if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedByte || indexAccessor.componentType == fastgltf::ComponentType::UnsignedShort) {
				primitive.indexType = GL_UNSIGNED_SHORT;
				glNamedBufferData(primitive.indexBuffer,
					static_cast<GLsizeiptr>(indexAccessor.count * sizeof(std::uint16_t)), nullptr,
					GL_STATIC_DRAW);
				auto* indices = static_cast<std::uint16_t*>(glMapNamedBuffer(primitive.indexBuffer, GL_WRITE_ONLY));
				fastgltf::copyFromAccessor<std::uint16_t>(asset.get(), indexAccessor, indices);
				glUnmapNamedBuffer(primitive.indexBuffer);
			} else {
				primitive.indexType = GL_UNSIGNED_INT;
				glNamedBufferData(primitive.indexBuffer,
					static_cast<GLsizeiptr>(indexAccessor.count * sizeof(std::uint32_t)), nullptr,
					GL_STATIC_DRAW);
				auto* indices = static_cast<std::uint32_t*>(glMapNamedBuffer(primitive.indexBuffer, GL_WRITE_ONLY));
				fastgltf::copyFromAccessor<std::uint32_t>(asset.get(), indexAccessor, indices);
				glUnmapNamedBuffer(primitive.indexBuffer);
			}

			glVertexArrayElementBuffer(vao, primitive.indexBuffer);
		}

		glCreateBuffers(1, &outMesh.drawsBuffer);
	    glNamedBufferData(outMesh.drawsBuffer, static_cast<GLsizeiptr>(outMesh.primitives.size() * sizeof(Primitive)),
	                      outMesh.primitives.data(), GL_STATIC_DRAW);

	    //arrput(meshes, outMesh);
	    meshes.emplace_back(outMesh);

	}


	//uint32_t id = LoadSkinnedMesh();
	return true;
}
