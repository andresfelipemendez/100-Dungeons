#define PositionType
#define RotationType
#define ColorType
#define CameraType
#define ModelType
#define MaterialType
#define InputType
#define VelocityType
#define ForceAccumulatorType
#define RigidBodyType
#define ColliderType
#define TextureType
#define MassType
#define GravityType

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

struct Position {
	float x;
	float y;
	float z;
};

struct Rotation {
	float x;
	float y;
	float z;
};

struct Color {
	float x;
	float y;
	float z;
};

struct Camera {
	float x;
	float y;
	float z;
};

struct Model {
	float x;
	float y;
	float z;
};

struct Material {
	float x;
	float y;
	float z;
};

struct Input {
	float x;
	float y;
	float z;
};

struct Velocity {
	float x;
	float y;
	float z;
};

struct ForceAccumulator {
	float x;
	float y;
	float z;
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
	float x;
	float y;
	float z;
};

struct Gravity {
	float x;
	float y;
	float z;
};

bool add_component(struct MemoryHeader *h, size_t entity_id, Position component);
bool add_component(struct MemoryHeader *h, size_t entity_id, Rotation component);
bool add_component(struct MemoryHeader *h, size_t entity_id, Color component);
bool add_component(struct MemoryHeader *h, size_t entity_id, Camera component);
bool add_component(struct MemoryHeader *h, size_t entity_id, Model component);
bool add_component(struct MemoryHeader *h, size_t entity_id, Material component);
bool add_component(struct MemoryHeader *h, size_t entity_id, Input component);
bool add_component(struct MemoryHeader *h, size_t entity_id, Velocity component);
bool add_component(struct MemoryHeader *h, size_t entity_id, ForceAccumulator component);
bool add_component(struct MemoryHeader *h, size_t entity_id, RigidBody component);
bool add_component(struct MemoryHeader *h, size_t entity_id, Collider component);
bool add_component(struct MemoryHeader *h, size_t entity_id, Texture component);
bool add_component(struct MemoryHeader *h, size_t entity_id, Mass component);
bool add_component(struct MemoryHeader *h, size_t entity_id, Gravity component);

bool get_component(struct MemoryHeader *h, size_t entity_id, Position *component);
bool get_component(struct MemoryHeader *h, size_t entity_id, Rotation *component);
bool get_component(struct MemoryHeader *h, size_t entity_id, Color *component);
bool get_component(struct MemoryHeader *h, size_t entity_id, Camera *component);
bool get_component(struct MemoryHeader *h, size_t entity_id, Model *component);
bool get_component(struct MemoryHeader *h, size_t entity_id, Material *component);
bool get_component(struct MemoryHeader *h, size_t entity_id, Input *component);
bool get_component(struct MemoryHeader *h, size_t entity_id, Velocity *component);
bool get_component(struct MemoryHeader *h, size_t entity_id, ForceAccumulator *component);
bool get_component(struct MemoryHeader *h, size_t entity_id, RigidBody *component);
bool get_component(struct MemoryHeader *h, size_t entity_id, Collider *component);
bool get_component(struct MemoryHeader *h, size_t entity_id, Texture *component);
bool get_component(struct MemoryHeader *h, size_t entity_id, Mass *component);
bool get_component(struct MemoryHeader *h, size_t entity_id, Gravity *component);

bool set_component(struct MemoryHeader *h, size_t entity_id, Position *component);
bool set_component(struct MemoryHeader *h, size_t entity_id, Rotation *component);
bool set_component(struct MemoryHeader *h, size_t entity_id, Color *component);
bool set_component(struct MemoryHeader *h, size_t entity_id, Camera *component);
bool set_component(struct MemoryHeader *h, size_t entity_id, Model *component);
bool set_component(struct MemoryHeader *h, size_t entity_id, Material *component);
bool set_component(struct MemoryHeader *h, size_t entity_id, Input *component);
bool set_component(struct MemoryHeader *h, size_t entity_id, Velocity *component);
bool set_component(struct MemoryHeader *h, size_t entity_id, ForceAccumulator *component);
bool set_component(struct MemoryHeader *h, size_t entity_id, RigidBody *component);
bool set_component(struct MemoryHeader *h, size_t entity_id, Collider *component);
bool set_component(struct MemoryHeader *h, size_t entity_id, Texture *component);
bool set_component(struct MemoryHeader *h, size_t entity_id, Mass *component);
bool set_component(struct MemoryHeader *h, size_t entity_id, Gravity *component);
