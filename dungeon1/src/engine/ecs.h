#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <glad.h>

#include <glm.hpp>

#define ENTITY_NAME_LENGTH 16

#define SUBKEY_TYPES                                                           \
	X(POSITION)                                                                \
	X(COLOR)                                                                   \
	X(SCALE)                                                                   \
	X(ROTATION)                                                                \
	X(CAMERA)                                                                  \
	X(FOV)                                                                     \
	X(INTENSITY)                                                               \
	X(MODEL)                                                                   \
	X(MATERIAL)                                                                \
	X(TEXTURE)

enum SubkeyType {
#define X(name) name##_TYPE,
	SUBKEY_TYPES
#undef X
		UNKNOWN_TYPE
};

SubkeyType mapStringToSubkeyType(const char *type_key);

#define EXPAND_AS_ENUM(name, index) name##_COMPONENT = (1 << index),

extern const char *component_names[];
extern size_t component_count;

enum ComponentBitmask {
#define X(name) EXPAND_AS_ENUM(name, __COUNTER__)
	SUBKEY_TYPES
#undef X
};

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

struct Transforms {
	size_t count;
	size_t *entity_ids;
	Vec3 *positions;
};

struct Rotations {
	size_t count;
	size_t *entity_ids;
	Vec3 *rotations;
};

typedef struct Models {
	size_t count;
	size_t *entity_ids;
	Vec3 *positions;
} Models;

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

typedef struct Textures {
	size_t count;
	size_t *entity_ids;
	Texture *textures;
} Textures;

struct Material {
	GLuint shader_id;
};

typedef struct Materials {
	size_t count;
	size_t *entity_ids;
	Material *materials;
} Materials;

typedef struct Shaders {
	size_t count;
	unsigned int *program_ids;
	char (*shader_names)[ENTITY_NAME_LENGTH];
} Shaders;

struct Camera {
	glm::mat4 projection;
	// float fov;
	// glm::vec3 position;
	// glm::vec3 rotation;
};

typedef struct Cameras {
	size_t count;
	size_t *entity_ids;
	Camera *cameras;
} Cameras;

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

struct MemoryHeader {
	// add arrays above to add components at runtime
	Materials *materials;
	Shaders *shaders;
	Cameras *cameras;
	Transforms *transforms;
	Meshes *meshes;
	ECSQuery query;
	World world;
	size_t total_size;
};

void ecs_load_level(struct game *g, const char *saveFilePath);
void save_level(MemoryHeader *h, const char *saveFilePath);

size_t create_entity(World *w);

bool get_entity(MemoryHeader *h, uint32_t component_mask,
				size_t &out_entity_id);

bool get_entities(MemoryHeader *h, uint32_t component_mask);

void set_entity_name(World *w, size_t entity, const char *friendly_name);

void add_component(MemoryHeader *h, size_t entity_id, uint32_t component_mask);

bool add_shader(MemoryHeader *h, char *name, GLuint programID);

bool get_component_value(MemoryHeader *h, size_t entity_id, Vec3 *value);
bool set_component_value(MemoryHeader *h, size_t entity_id, Vec3 value);

bool get_component_value(MemoryHeader *h, size_t entity_id, Camera *value);
bool set_component_value(MemoryHeader *h, size_t entity_id, Camera value);

bool get_component_value(MemoryHeader *h, size_t entity, StaticMesh *value);
bool set_component_value(MemoryHeader *h, size_t entity, StaticMesh value);

bool get_component_value(MemoryHeader *h, size_t entity, Material *value);
bool set_component_value(MemoryHeader *h, size_t entity, Material value);
bool check_entity_component(MemoryHeader *h, size_t entity,
							uint32_t component_mask);
bool get_entity_name(World *w, size_t entity, char *name);