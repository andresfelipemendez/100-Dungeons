#include "memory.h"
#include <cstring>
#define WIN32_LEAN_AND_MEAN
#include "asset_loader.h"

#include "ecs.h"
#include <cstdio>
#include <cstdlib>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <glad.h>

#include <printLog.h>

#include <Windows.h>
#include <minwinbase.h>
#include <winnt.h>

bool LoadGLTFMeshes(MemoryHeader *h, const char *meshFilePath,
					StaticMesh *outMesh) {
	constexpr auto gltfOptions = fastgltf::Options::LoadExternalBuffers |
								 fastgltf::Options::LoadExternalImages |
								 fastgltf::Options::GenerateMeshIndices;
	fastgltf::Parser parser;

	std::filesystem::path fullMeshPath =
		std::filesystem::current_path() / meshFilePath;

	fullMeshPath.make_preferred();

	auto data = fastgltf::GltfDataBuffer::FromPath(fullMeshPath);
	if (data.error() != fastgltf::Error::None) {
		print_log(COLOR_RED, "error loading model");
		return false;
	}

	auto asset =
		parser.loadGltf(data.get(), fullMeshPath.parent_path(), gltfOptions);
	if (auto error = asset.error(); error != fastgltf::Error::None) {
		char buffer[256];
		snprintf(buffer, sizeof(buffer),
				 "Error loading GLTF data from file: %s (Error Code: %s)",
				 fullMeshPath.string().c_str(),
				 fastgltf::getErrorMessage(error).data());

		print_log(COLOR_RED, buffer);
		return false;
	}

	*outMesh = h->meshes->mesh_data[h->meshes->count++];

	printf("h->meshes->mesh_data count %zu\n", h->meshes->count);

	for (auto &mesh : asset.get().meshes) {
		for (auto it = mesh.primitives.begin(); it != mesh.primitives.end();
			 ++it) {

			outMesh->submesh_count++;

			if (it->type != fastgltf::PrimitiveType::Triangles) {
				print_log(COLOR_RED, "submesh type it's not GL_TRIANGLES\n");
				return false;
			}

			GLuint vao = GL_NONE;
			glCreateVertexArrays(1, &vao);
			size_t baseColorTexcoordIndex = 0;

			auto *positionIt = it->findAttribute("POSITION");
			auto index = std::distance(mesh.primitives.begin(), it);

			SubMesh *subMesh = &outMesh->submeshes[index];
			subMesh->vertexArray = vao;
			auto &positionAccessor =
				asset.get().accessors[positionIt->accessorIndex];
			if (!positionAccessor.bufferViewIndex.has_value())
				continue;

			glCreateBuffers(1, &subMesh->vertexBuffer);

			glNamedBufferData(subMesh->vertexBuffer,
							  positionAccessor.count * sizeof(Vertex), nullptr,
							  GL_STATIC_DRAW);

			auto *vertices = static_cast<Vertex *>(
				glMapNamedBuffer(subMesh->vertexBuffer, GL_WRITE_ONLY));

			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
				asset.get(), positionAccessor,
				[&](fastgltf::math::fvec3 pos, std::size_t idx) {
					vertices[idx].position = Vec3{pos.x(), pos.y(), pos.z()};
				});

			glUnmapNamedBuffer(subMesh->vertexBuffer);

			glEnableVertexArrayAttrib(vao, 0);
			glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
			glVertexArrayAttribBinding(vao, 0, 0);
			glVertexArrayVertexBuffer(vao, 0, subMesh->vertexBuffer, 0,
									  sizeof(Vertex));

			auto texcoordAttribute = std::string("TEXCOORD_") +
									 std::to_string(baseColorTexcoordIndex);

			if (const auto *texcoord = it->findAttribute(texcoordAttribute);
				texcoord != it->attributes.end()) {
				auto &texCoordAccessor =
					asset.get().accessors[texcoord->accessorIndex];
				if (!texCoordAccessor.bufferViewIndex.has_value())
					continue;
				auto *vertices = static_cast<Vertex *>(
					glMapNamedBuffer(subMesh->vertexBuffer, GL_WRITE_ONLY));
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
					asset.get(), texCoordAccessor,
					[&](fastgltf::math::fvec2 uv, std::size_t idx) {
						vertices[idx].uv = Vec2{uv.x(), uv.y()};
					});
				glUnmapNamedBuffer(subMesh->vertexBuffer);
				glEnableVertexArrayAttrib(vao, 1);
				glVertexArrayAttribFormat(vao, 1, 2, GL_FLOAT, GL_FALSE, 0);
				glVertexArrayAttribBinding(vao, 1, 1);
				glVertexArrayVertexBuffer(vao, 1, subMesh->vertexBuffer,
										  offsetof(Vertex, uv), sizeof(Vertex));
			}

			auto &draw = subMesh->draw;
			draw.instanceCount = 1;
			draw.baseInstance = 0;
			draw.baseVertex = 0;
			draw.firstIndex = 0;

			auto &indexAccessor =
				asset.get().accessors[it->indicesAccessor.value()];
			if (!indexAccessor.bufferViewIndex.has_value())
				return false;
			draw.count = static_cast<uint32_t>(indexAccessor.count);
			glCreateBuffers(1, &subMesh->indexBuffer);

			if (indexAccessor.componentType ==
					fastgltf::ComponentType::UnsignedByte ||
				indexAccessor.componentType ==
					fastgltf::ComponentType::UnsignedShort) {
				subMesh->indexType = GL_UNSIGNED_SHORT;
				glNamedBufferData(
					subMesh->indexBuffer,
					static_cast<GLsizeiptr>(indexAccessor.count *
											sizeof(std::uint16_t)),
					nullptr, GL_STATIC_DRAW);
				auto *indices = static_cast<std::uint16_t *>(
					glMapNamedBuffer(subMesh->indexBuffer, GL_WRITE_ONLY));
				fastgltf::copyFromAccessor<std::uint16_t>(
					asset.get(), indexAccessor, indices);
				glUnmapNamedBuffer(subMesh->indexBuffer);
			} else {
				subMesh->indexType = GL_UNSIGNED_INT;
				glNamedBufferData(
					subMesh->indexBuffer,
					static_cast<GLsizeiptr>(indexAccessor.count *
											sizeof(std::uint32_t)),
					nullptr, GL_STATIC_DRAW);
				auto *indices = static_cast<std::uint32_t *>(
					glMapNamedBuffer(subMesh->indexBuffer, GL_WRITE_ONLY));
				fastgltf::copyFromAccessor<std::uint32_t>(
					asset.get(), indexAccessor, indices);
				glUnmapNamedBuffer(subMesh->indexBuffer);
			}

			glVertexArrayElementBuffer(vao, subMesh->indexBuffer);
			glCreateBuffers(1, &outMesh->drawsBuffer);

			glNamedBufferData(outMesh->drawsBuffer,
							  static_cast<GLsizeiptr>(outMesh->submesh_count *
													  sizeof(SubMesh)),
							  outMesh->submeshes, GL_STATIC_DRAW);
		}
	}

	return true;
}

unsigned int createShaderProgram(const char *vertexSource,
								 const char *fragmentShaderSource) {
	unsigned int vertexShader, fragmentShader;
	int success;
	char infoLog[512];

	// Vertex Shader
	vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	glCompileShader(vertexShader);
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		fprintf(stderr, "Vertex Shader Compilation Error:\n%s\n", infoLog);
	}

	// Fragment Shader
	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		fprintf(stderr, "Fragment Shader Compilation Error:\n%s\n", infoLog);
	}

	// Shader Program
	unsigned int program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(program, 512, NULL, infoLog);
		fprintf(stderr, "Shader Program Linking Error:\n%s\n", infoLog);
	}

	// Clean up shaders as they are no longer needed after linking
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return program;
}

char *read_file(const char *filepath) {
	FILE *file = NULL;
	errno_t err = fopen_s(&file, filepath, "rb");
	if (err != 0 || file == NULL) {
		printf("Failed to open file: %s\n", filepath);
	}
	fseek(file, 0, SEEK_END);
	long length = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *content = (char *)malloc(length + 1);
	if (!content) {
		printf("Failed to allocate memory for the shader file content\n");
		fclose(file);
		return nullptr;
	}

	fread(content, 1, length, file);
	content[length] = '\0';
	fclose(file);
	return content;
}

void load_shaders(struct game *g) {
	char cwd[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, cwd);

	char shaderDir[MAX_PATH];
	snprintf(shaderDir, sizeof(shaderDir), "%s\\assets\\shaders", cwd);

	char searchPath[MAX_PATH];
	snprintf(searchPath, sizeof(searchPath), "%s\\*.vert", shaderDir);

	WIN32_FIND_DATA findFileData;
	HANDLE hFind = FindFirstFile(searchPath, &findFileData);

	if (hFind == INVALID_HANDLE_VALUE) {
		printf("Failed to open shader directory: %s\n", shaderDir);
		return;
	}

	MemoryHeader *h = get_header(g);
	do {
		const char *vertFileName = findFileData.cFileName;

		char shaderName[MAX_PATH];
		strncpy_s(shaderName, sizeof(shaderName), vertFileName,
				  strlen(vertFileName) - 5);

		char vertPath[MAX_PATH];
		snprintf(vertPath, sizeof(vertPath), "%s\\%s", shaderDir, vertFileName);
		char fragPath[MAX_PATH];
		strncpy_s(fragPath, sizeof(fragPath), vertPath, strlen(vertPath) - 5);
		strcat_s(fragPath, sizeof(fragPath), ".frag");

		char *vertexSource = read_file(vertPath);
		char *fragmentSource = read_file(fragPath);

		if (vertexSource && fragmentSource) {
			GLuint shaderProgram =
				createShaderProgram(vertexSource, fragmentSource);

			free(vertexSource);
			free(fragmentSource);
			printf("shader %s program id is: %i\n", shaderName, shaderProgram);
			add_shader(h, shaderName, shaderProgram);
		}

	} while (FindNextFile(hFind, &findFileData) != 0);

	FindClose(hFind);
}