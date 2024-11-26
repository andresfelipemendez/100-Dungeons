#include <glm.hpp>
#include <glad.h>
enum ComponentType {
	PositionType,
	RotationType,
	ColorType,
	CameraType,
	ModelType,
	MaterialType,
	InputType,
	VelocityType,
	ForceAccumulatorType,
	RigidBodyType,
	ColliderType,
	TextureType,
	MassType,
	GravityType,
	UNKNOWN_TYPE
};

enum ComponentBitmask {
	PositionComponent = (1 << 0),
	RotationComponent = (1 << 1),
	ColorComponent = (1 << 2),
	CameraComponent = (1 << 3),
	ModelComponent = (1 << 4),
	MaterialComponent = (1 << 5),
	InputComponent = (1 << 6),
	VelocityComponent = (1 << 7),
	ForceAccumulatorComponent = (1 << 8),
	RigidBodyComponent = (1 << 9),
	ColliderComponent = (1 << 10),
	TextureComponent = (1 << 11),
	MassComponent = (1 << 12),
	GravityComponent = (1 << 13),
};

extern const char *component_names[];
extern size_t component_count;

struct Position {
	float x;
	float y;
	float z;
};

struct Rotation {
	float x;
	float y;
	float z;
	float w;
};

struct Color {
	float x;
	float y;
	float z;
};

struct Camera {
	float fov;
	float near;
	float far;
};

struct Model {
	float submesh_count;
};

struct Material {
	GLuint shader_id;
};

struct Input {
	float x;
	float y;
	float z;
};

struct Velocity {
	glm::vec3 linear;
	glm::vec3 angular;
};

struct ForceAccumulator {
	glm::vec3 force;
	glm::vec3 torque;
};

struct RigidBody {
	float x;
	float y;
	float z;
};

struct Collider {
	float x;
	float y;
	float z;
};

struct Texture {
	float x;
	float y;
	float z;
};

struct Mass {
	float inv;
};

struct Gravity {
	float value;
};

typedef struct Positions {
	size_t count;
	size_t *entity_ids;
	Position *components;
} Positions;

typedef struct Rotations {
	size_t count;
	size_t *entity_ids;
	Rotation *components;
} Rotations;

typedef struct Colors {
	size_t count;
	size_t *entity_ids;
	Color *components;
} Colors;

typedef struct Cameras {
	size_t count;
	size_t *entity_ids;
	Camera *components;
} Cameras;

typedef struct Models {
	size_t count;
	size_t *entity_ids;
	Model *components;
} Models;

typedef struct Materials {
	size_t count;
	size_t *entity_ids;
	Material *components;
} Materials;

typedef struct Inputs {
	size_t count;
	size_t *entity_ids;
	Input *components;
} Inputs;

typedef struct Velocitys {
	size_t count;
	size_t *entity_ids;
	Velocity *components;
} Velocitys;

typedef struct ForceAccumulators {
	size_t count;
	size_t *entity_ids;
	ForceAccumulator *components;
} ForceAccumulators;

typedef struct RigidBodys {
	size_t count;
	size_t *entity_ids;
	RigidBody *components;
} RigidBodys;

typedef struct Colliders {
	size_t count;
	size_t *entity_ids;
	Collider *components;
} Colliders;

typedef struct Textures {
	size_t count;
	size_t *entity_ids;
	Texture *components;
} Textures;

typedef struct Masss {
	size_t count;
	size_t *entity_ids;
	Mass *components;
} Masss;

typedef struct Gravitys {
	size_t count;
	size_t *entity_ids;
	Gravity *components;
} Gravitys;

struct Components {
	Positions *pPositions;
	Rotations *pRotations;
	Colors *pColors;
	Cameras *pCameras;
	Models *pModels;
	Materials *pMaterials;
	Inputs *pInputs;
	Velocitys *pVelocitys;
	ForceAccumulators *pForceAccumulators;
	RigidBodys *pRigidBodys;
	Colliders *pColliders;
	Textures *pTextures;
	Masss *pMasss;
	Gravitys *pGravitys;
};

ComponentType mapStringToComponentType(const char * type_key);
void add_component(Components *h, size_t entity_id, Position component);
void add_component(Components *h, size_t entity_id, Rotation component);
void add_component(Components *h, size_t entity_id, Color component);
void add_component(Components *h, size_t entity_id, Camera component);
void add_component(Components *h, size_t entity_id, Model component);
void add_component(Components *h, size_t entity_id, Material component);
void add_component(Components *h, size_t entity_id, Input component);
void add_component(Components *h, size_t entity_id, Velocity component);
void add_component(Components *h, size_t entity_id, ForceAccumulator component);
void add_component(Components *h, size_t entity_id, RigidBody component);
void add_component(Components *h, size_t entity_id, Collider component);
void add_component(Components *h, size_t entity_id, Texture component);
void add_component(Components *h, size_t entity_id, Mass component);
void add_component(Components *h, size_t entity_id, Gravity component);

bool get_component(Components *h, size_t entity_id, Position *component);
bool get_component(Components *h, size_t entity_id, Rotation *component);
bool get_component(Components *h, size_t entity_id, Color *component);
bool get_component(Components *h, size_t entity_id, Camera *component);
bool get_component(Components *h, size_t entity_id, Model *component);
bool get_component(Components *h, size_t entity_id, Material *component);
bool get_component(Components *h, size_t entity_id, Input *component);
bool get_component(Components *h, size_t entity_id, Velocity *component);
bool get_component(Components *h, size_t entity_id, ForceAccumulator *component);
bool get_component(Components *h, size_t entity_id, RigidBody *component);
bool get_component(Components *h, size_t entity_id, Collider *component);
bool get_component(Components *h, size_t entity_id, Texture *component);
bool get_component(Components *h, size_t entity_id, Mass *component);
bool get_component(Components *h, size_t entity_id, Gravity *component);

bool set_component(Components *h, size_t entity_id, Position *component);
bool set_component(Components *h, size_t entity_id, Rotation *component);
bool set_component(Components *h, size_t entity_id, Color *component);
bool set_component(Components *h, size_t entity_id, Camera *component);
bool set_component(Components *h, size_t entity_id, Model *component);
bool set_component(Components *h, size_t entity_id, Material *component);
bool set_component(Components *h, size_t entity_id, Input *component);
bool set_component(Components *h, size_t entity_id, Velocity *component);
bool set_component(Components *h, size_t entity_id, ForceAccumulator *component);
bool set_component(Components *h, size_t entity_id, RigidBody *component);
bool set_component(Components *h, size_t entity_id, Collider *component);
bool set_component(Components *h, size_t entity_id, Texture *component);
bool set_component(Components *h, size_t entity_id, Mass *component);
bool set_component(Components *h, size_t entity_id, Gravity *component);
