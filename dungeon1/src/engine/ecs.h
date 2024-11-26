#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <glad.h>

#include <glm.hpp>

#define ENTITY_NAME_LENGTH 16

// struct Gravity {
//   float value;
// };

// struct Velocity {
//   glm::vec3 linear;
//   glm::vec3 angular;
// };

// struct Mass {
//   float inv;
// };

// struct RigidBody {
//   float restitution;
//   float friction;
//   float linearDamping;
//   float angularDamping;
// };
// struct ForceAccumulator {
//   glm::vec3 force;
//   glm::vec3 torque;
// };

// struct Rotation {
//   float x;
//   float y;
//   float z;
//   float w;
// };

// struct Color {};
// struct Collider {};

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

// struct Model {
//   size_t entity_id;
//   size_t submesh_count;
//   GLuint drawsBuffer;
//   SubMesh *submeshes;
// };

// struct Fov {};
// struct Input {};

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

// struct Transform {};

// typedef struct StaticMesh {
//   size_t entity_id;
//   size_t submesh_count;
//   GLuint drawsBuffer;
//   SubMesh *submeshes;
// } StaticMesh;

// struct Meshes {
//   size_t count;
//   size_t *entity_ids;
//   StaticMesh *mesh_data;
// };

// typedef struct Texture {
//   unsigned int textureID;
//   int texWidth;
//   int texHeight;
// } Texture;


typedef struct Shaders {
  size_t count;
  unsigned int *program_ids;
  char (*shader_names)[ENTITY_NAME_LENGTH];
} Shaders;

// struct Camera {
//   float fov;
//   float near;
//   float far;
// };

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

typedef struct Memory {
  struct Components *components;
  Shaders *shaders;
  ECSQuery query;
  World world;
  size_t total_size;
} Memory;

void ecs_load_level(struct game *g, const char *saveFilePath);
void save_level(struct Memory *h, const char *saveFilePath);

size_t create_entity(World *w);

bool get_entity(struct Memory *h, uint32_t component_mask,
                size_t &out_entity_id);

bool get_entities(struct Memory *h, uint32_t component_mask);

void set_entity_name(World *w, size_t entity, const char *friendly_name);

bool check_entity_component(Memory *m, size_t entity, uint32_t component_mask);
bool get_entity_name(World *w, size_t entity, char *name);

bool add_shader(struct Memory *h, char *name, GLuint programID);
