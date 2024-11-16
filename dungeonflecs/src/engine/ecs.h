#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <glad.h>

#include <glm.hpp>

#define ENTITY_NAME_LENGTH 16

#define SUBKEY_TYPES                                                           \
	X(Position)                                                                \
	X(Rotation)                                                                \
	X(Color)                                                                   \
	X(Camera)                                                                  \
	X(Model)                                                                   \
	X(Material)                                                                \
	X(Input)                                                                   \
	X(Texture)

enum SubkeyType {
#define X(name) name##Type,
	SUBKEY_TYPES
#undef X
		UNKNOWN_TYPE
};

SubkeyType mapStringToSubkeyType(const char *type_key);

#define EXPAND_AS_ENUM(name, index) name##Component = (1 << index),

extern const char *component_names[];
extern size_t component_count;

enum ComponentBitmask {
#define X(name) EXPAND_AS_ENUM(name, __COUNTER__)
	SUBKEY_TYPES
#undef X
};

struct Position {
	float x;
	float y;
	float z;
};

struct Rotation {
	float pitch;
	float yaw;
	float roll;
};

struct Color {};

typedef struct IndirectDrawCommand {
	uint32_t count;
	uint32_t instanceCount;
	uint32_t firstIndex;
	int32_t baseVertex;
	uint32_t baseInstance;
} IndirectDrawCommand;

typedef struct SubMesh {
	IndirectDrawCommand draw;
	unsigned int vertexArray;
	unsigned int vertexBuffer;
	unsigned int indexBuffer;
	GLenum indexType;
} SubMesh;

struct Model {
	size_t entity_id;
	size_t submesh_count;
	GLuint drawsBuffer;
	SubMesh *submeshes;
};
struct Fov {};
struct Input {};

typedef union Vec3 {
	struct {
		float x, y, z;
	};
	struct {
		float r, g, b;
	};
	struct {
		float pitch, yaw, roll;
	};
	float data[3];
} Vec3;

typedef union Vec2 {
	struct {
		float x, y;
	};
	struct {
		float u, v;
	};
	float data[2];
} Vec2;

typedef struct Vertex {
	Vec3 position;
	Vec3 normal;
	Vec2 uv;
} Vertex;

struct Transform {};

typedef struct StaticMesh {
	size_t entity_id;
	size_t submesh_count;
	GLuint drawsBuffer;
	SubMesh *submeshes;
} StaticMesh;

struct Meshes {
	size_t count;
	size_t *entity_ids;
	StaticMesh *mesh_data;
};

typedef struct Texture {
	unsigned int textureID;
	int texWidth;
	int texHeight;
} Texture;

struct Material {
	GLuint shader_id;
};

typedef struct Shaders {
	size_t count;
	unsigned int *program_ids;
	char (*shader_names)[ENTITY_NAME_LENGTH];
} Shaders;

struct Camera {
	float fov;
	float near;
	float far;
};

typedef struct World {
	size_t entity_count;
	size_t *entity_ids;
	uint32_t *component_masks;
	char (*entity_names)[ENTITY_NAME_LENGTH];
} World;

struct ECSQuery {
	size_t count;
	size_t *entities;
};

#define DEFINE_COMPONENT_STRUCT(name)                                          \
	typedef struct name##s {                                                   \
		size_t count;                                                          \
		size_t *entity_ids;                                                    \
		name *components;                                                      \
	} name##s;

#define X(name) DEFINE_COMPONENT_STRUCT(name)
SUBKEY_TYPES
#undef X

#undef DEFINE_COMPONENT_STRUCT

#define MEMORY_HEADER_COMPONENT(name) name##s *p##name##s;

struct MemoryHeader {
#define X(name) MEMORY_HEADER_COMPONENT(name)
	SUBKEY_TYPES
#undef X
	Shaders *shaders;
	ECSQuery query;
	World world;
	size_t total_size;
};

#undef MEMORY_HEADER_COMPONENT

void ecs_load_level(struct game *g, const char *saveFilePath);
void save_level(MemoryHeader *h, const char *saveFilePath);

size_t create_entity(World *w);

bool get_entity(MemoryHeader *h, uint32_t component_mask,
				size_t &out_entity_id);

bool get_entities(MemoryHeader *h, uint32_t component_mask);

void set_entity_name(World *w, size_t entity, const char *friendly_name);

bool check_entity_component(MemoryHeader *h, size_t entity,
							uint32_t component_mask);
bool get_entity_name(World *w, size_t entity, char *name);

bool add_shader(MemoryHeader *h, char *name, GLuint programID);

#define DECLARE_ADD_COMPONENT_FUNCTION(name)                                   \
	void add_component(MemoryHeader *h, size_t entity_id, name component);
#define X(name) DECLARE_ADD_COMPONENT_FUNCTION(name)
SUBKEY_TYPES
#undef X
#undef DECLARE_ADD_COMPONENT_FUNCTION

#define DECLARE_GET_COMPONENT_FUNCTION(name)                                   \
	bool get_component(MemoryHeader *h, size_t entity_id, name *component);

#define X(name) DECLARE_GET_COMPONENT_FUNCTION(name)
SUBKEY_TYPES
#undef X
#undef DECLARE_GET_COMPONENT_FUNCTION

#define DECLARE_SET_COMPONENT_FUNCTION(name)                                   \
	bool set_component(MemoryHeader *h, size_t entity_id, name component);

#define X(name) DECLARE_SET_COMPONENT_FUNCTION(name)
SUBKEY_TYPES
#undef X
#undef DECLARE_SET_COMPONENT_FUNCTION
