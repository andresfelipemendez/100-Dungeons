#include "components.h"
#include <toml.h>
#include <game.h>
#include "memory.h"

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

extern size_t component_count;
size_t component_count = 14;

const char *component_names[] = {
	"Position",
	"Rotation",
	"Color",
	"Camera",
	"Model",
	"Material",
	"Input",
	"Velocity",
	"ForceAccumulator",
	"RigidBody",
	"Collider",
	"Texture",
	"Mass",
	"Gravity",
};

ComponentType mapStringToComponentType(const char * type_key){
	if(strcasecmp(type_key, "Position") == 0) return PositionType;
	if(strcasecmp(type_key, "Rotation") == 0) return RotationType;
	if(strcasecmp(type_key, "Color") == 0) return ColorType;
	if(strcasecmp(type_key, "Camera") == 0) return CameraType;
	if(strcasecmp(type_key, "Model") == 0) return ModelType;
	if(strcasecmp(type_key, "Material") == 0) return MaterialType;
	if(strcasecmp(type_key, "Input") == 0) return InputType;
	if(strcasecmp(type_key, "Velocity") == 0) return VelocityType;
	if(strcasecmp(type_key, "ForceAccumulator") == 0) return ForceAccumulatorType;
	if(strcasecmp(type_key, "RigidBody") == 0) return RigidBodyType;
	if(strcasecmp(type_key, "Collider") == 0) return ColliderType;
	if(strcasecmp(type_key, "Texture") == 0) return TextureType;
	if(strcasecmp(type_key, "Mass") == 0) return MassType;
	if(strcasecmp(type_key, "Gravity") == 0) return GravityType;
	return UNKNOWN_TYPE;
}
void assign_components_memory(Memory *m, game* g, size_t* offset) {
	m->components = (Components *)((char *)g->buffer + *offset);
	*offset += sizeof(Components);
	m->components->pPositions = (Positions *)((char *)g->buffer + *offset);
	*offset += sizeof(Positions);
	m->components->pPositions->entity_ids = (size_t *)((char *)g->buffer + *offset);
	*offset += sizeof(size_t) * initialEntityCount;
	m->components->pPositions->components = (Position *)((char *)g->buffer + *offset);
	*offset += sizeof(Position) * initialEntityCount;
	m->components->pRotations = (Rotations *)((char *)g->buffer + *offset);
	*offset += sizeof(Rotations);
	m->components->pRotations->entity_ids = (size_t *)((char *)g->buffer + *offset);
	*offset += sizeof(size_t) * initialEntityCount;
	m->components->pRotations->components = (Rotation *)((char *)g->buffer + *offset);
	*offset += sizeof(Rotation) * initialEntityCount;
	m->components->pColors = (Colors *)((char *)g->buffer + *offset);
	*offset += sizeof(Colors);
	m->components->pColors->entity_ids = (size_t *)((char *)g->buffer + *offset);
	*offset += sizeof(size_t) * initialEntityCount;
	m->components->pColors->components = (Color *)((char *)g->buffer + *offset);
	*offset += sizeof(Color) * initialEntityCount;
	m->components->pCameras = (Cameras *)((char *)g->buffer + *offset);
	*offset += sizeof(Cameras);
	m->components->pCameras->entity_ids = (size_t *)((char *)g->buffer + *offset);
	*offset += sizeof(size_t) * initialEntityCount;
	m->components->pCameras->components = (Camera *)((char *)g->buffer + *offset);
	*offset += sizeof(Camera) * initialEntityCount;
	m->components->pModels = (Models *)((char *)g->buffer + *offset);
	*offset += sizeof(Models);
	m->components->pModels->entity_ids = (size_t *)((char *)g->buffer + *offset);
	*offset += sizeof(size_t) * initialEntityCount;
	m->components->pModels->components = (Model *)((char *)g->buffer + *offset);
	*offset += sizeof(Model) * initialEntityCount;
	m->components->pMaterials = (Materials *)((char *)g->buffer + *offset);
	*offset += sizeof(Materials);
	m->components->pMaterials->entity_ids = (size_t *)((char *)g->buffer + *offset);
	*offset += sizeof(size_t) * initialEntityCount;
	m->components->pMaterials->components = (Material *)((char *)g->buffer + *offset);
	*offset += sizeof(Material) * initialEntityCount;
	m->components->pInputs = (Inputs *)((char *)g->buffer + *offset);
	*offset += sizeof(Inputs);
	m->components->pInputs->entity_ids = (size_t *)((char *)g->buffer + *offset);
	*offset += sizeof(size_t) * initialEntityCount;
	m->components->pInputs->components = (Input *)((char *)g->buffer + *offset);
	*offset += sizeof(Input) * initialEntityCount;
	m->components->pVelocitys = (Velocitys *)((char *)g->buffer + *offset);
	*offset += sizeof(Velocitys);
	m->components->pVelocitys->entity_ids = (size_t *)((char *)g->buffer + *offset);
	*offset += sizeof(size_t) * initialEntityCount;
	m->components->pVelocitys->components = (Velocity *)((char *)g->buffer + *offset);
	*offset += sizeof(Velocity) * initialEntityCount;
	m->components->pForceAccumulators = (ForceAccumulators *)((char *)g->buffer + *offset);
	*offset += sizeof(ForceAccumulators);
	m->components->pForceAccumulators->entity_ids = (size_t *)((char *)g->buffer + *offset);
	*offset += sizeof(size_t) * initialEntityCount;
	m->components->pForceAccumulators->components = (ForceAccumulator *)((char *)g->buffer + *offset);
	*offset += sizeof(ForceAccumulator) * initialEntityCount;
	m->components->pRigidBodys = (RigidBodys *)((char *)g->buffer + *offset);
	*offset += sizeof(RigidBodys);
	m->components->pRigidBodys->entity_ids = (size_t *)((char *)g->buffer + *offset);
	*offset += sizeof(size_t) * initialEntityCount;
	m->components->pRigidBodys->components = (RigidBody *)((char *)g->buffer + *offset);
	*offset += sizeof(RigidBody) * initialEntityCount;
	m->components->pColliders = (Colliders *)((char *)g->buffer + *offset);
	*offset += sizeof(Colliders);
	m->components->pColliders->entity_ids = (size_t *)((char *)g->buffer + *offset);
	*offset += sizeof(size_t) * initialEntityCount;
	m->components->pColliders->components = (Collider *)((char *)g->buffer + *offset);
	*offset += sizeof(Collider) * initialEntityCount;
	m->components->pTextures = (Textures *)((char *)g->buffer + *offset);
	*offset += sizeof(Textures);
	m->components->pTextures->entity_ids = (size_t *)((char *)g->buffer + *offset);
	*offset += sizeof(size_t) * initialEntityCount;
	m->components->pTextures->components = (Texture *)((char *)g->buffer + *offset);
	*offset += sizeof(Texture) * initialEntityCount;
	m->components->pMasss = (Masss *)((char *)g->buffer + *offset);
	*offset += sizeof(Masss);
	m->components->pMasss->entity_ids = (size_t *)((char *)g->buffer + *offset);
	*offset += sizeof(size_t) * initialEntityCount;
	m->components->pMasss->components = (Mass *)((char *)g->buffer + *offset);
	*offset += sizeof(Mass) * initialEntityCount;
	m->components->pGravitys = (Gravitys *)((char *)g->buffer + *offset);
	*offset += sizeof(Gravitys);
	m->components->pGravitys->entity_ids = (size_t *)((char *)g->buffer + *offset);
	*offset += sizeof(size_t) * initialEntityCount;
	m->components->pGravitys->components = (Gravity *)((char *)g->buffer + *offset);
	*offset += sizeof(Gravity) * initialEntityCount;
}

void add_component(Memory *m, size_t entity_id, Position component) {
	size_t i = m->components->pPositions->count;
	m->components->pPositions->entity_ids[i] = entity_id;
	m->components->pPositions->components[i] = component;
	m->components->pPositions->count++;
	m->world.component_masks[entity_id] |= PositionComponent;
}

void add_component(Memory *m, size_t entity_id, Rotation component) {
	size_t i = m->components->pRotations->count;
	m->components->pRotations->entity_ids[i] = entity_id;
	m->components->pRotations->components[i] = component;
	m->components->pRotations->count++;
	m->world.component_masks[entity_id] |= RotationComponent;
}

void add_component(Memory *m, size_t entity_id, Color component) {
	size_t i = m->components->pColors->count;
	m->components->pColors->entity_ids[i] = entity_id;
	m->components->pColors->components[i] = component;
	m->components->pColors->count++;
	m->world.component_masks[entity_id] |= ColorComponent;
}

void add_component(Memory *m, size_t entity_id, Camera component) {
	size_t i = m->components->pCameras->count;
	m->components->pCameras->entity_ids[i] = entity_id;
	m->components->pCameras->components[i] = component;
	m->components->pCameras->count++;
	m->world.component_masks[entity_id] |= CameraComponent;
}

void add_component(Memory *m, size_t entity_id, Model component) {
	size_t i = m->components->pModels->count;
	m->components->pModels->entity_ids[i] = entity_id;
	m->components->pModels->components[i] = component;
	m->components->pModels->count++;
	m->world.component_masks[entity_id] |= ModelComponent;
}

void add_component(Memory *m, size_t entity_id, Material component) {
	size_t i = m->components->pMaterials->count;
	m->components->pMaterials->entity_ids[i] = entity_id;
	m->components->pMaterials->components[i] = component;
	m->components->pMaterials->count++;
	m->world.component_masks[entity_id] |= MaterialComponent;
}

void add_component(Memory *m, size_t entity_id, Input component) {
	size_t i = m->components->pInputs->count;
	m->components->pInputs->entity_ids[i] = entity_id;
	m->components->pInputs->components[i] = component;
	m->components->pInputs->count++;
	m->world.component_masks[entity_id] |= InputComponent;
}

void add_component(Memory *m, size_t entity_id, Velocity component) {
	size_t i = m->components->pVelocitys->count;
	m->components->pVelocitys->entity_ids[i] = entity_id;
	m->components->pVelocitys->components[i] = component;
	m->components->pVelocitys->count++;
	m->world.component_masks[entity_id] |= VelocityComponent;
}

void add_component(Memory *m, size_t entity_id, ForceAccumulator component) {
	size_t i = m->components->pForceAccumulators->count;
	m->components->pForceAccumulators->entity_ids[i] = entity_id;
	m->components->pForceAccumulators->components[i] = component;
	m->components->pForceAccumulators->count++;
	m->world.component_masks[entity_id] |= ForceAccumulatorComponent;
}

void add_component(Memory *m, size_t entity_id, RigidBody component) {
	size_t i = m->components->pRigidBodys->count;
	m->components->pRigidBodys->entity_ids[i] = entity_id;
	m->components->pRigidBodys->components[i] = component;
	m->components->pRigidBodys->count++;
	m->world.component_masks[entity_id] |= RigidBodyComponent;
}

void add_component(Memory *m, size_t entity_id, Collider component) {
	size_t i = m->components->pColliders->count;
	m->components->pColliders->entity_ids[i] = entity_id;
	m->components->pColliders->components[i] = component;
	m->components->pColliders->count++;
	m->world.component_masks[entity_id] |= ColliderComponent;
}

void add_component(Memory *m, size_t entity_id, Texture component) {
	size_t i = m->components->pTextures->count;
	m->components->pTextures->entity_ids[i] = entity_id;
	m->components->pTextures->components[i] = component;
	m->components->pTextures->count++;
	m->world.component_masks[entity_id] |= TextureComponent;
}

void add_component(Memory *m, size_t entity_id, Mass component) {
	size_t i = m->components->pMasss->count;
	m->components->pMasss->entity_ids[i] = entity_id;
	m->components->pMasss->components[i] = component;
	m->components->pMasss->count++;
	m->world.component_masks[entity_id] |= MassComponent;
}

void add_component(Memory *m, size_t entity_id, Gravity component) {
	size_t i = m->components->pGravitys->count;
	m->components->pGravitys->entity_ids[i] = entity_id;
	m->components->pGravitys->components[i] = component;
	m->components->pGravitys->count++;
	m->world.component_masks[entity_id] |= GravityComponent;
}

bool get_component(Memory *m, size_t entity_id, Position *component) {
	if (!check_entity_component(m, entity_id, PositionComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pPositions->count; i++) {
		if (m->components->pPositions->entity_ids[i] == entity_id) {
			*component = m->components->pPositions->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(Memory *m, size_t entity_id, Rotation *component) {
	if (!check_entity_component(m, entity_id, RotationComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pRotations->count; i++) {
		if (m->components->pRotations->entity_ids[i] == entity_id) {
			*component = m->components->pRotations->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(Memory *m, size_t entity_id, Color *component) {
	if (!check_entity_component(m, entity_id, ColorComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pColors->count; i++) {
		if (m->components->pColors->entity_ids[i] == entity_id) {
			*component = m->components->pColors->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(Memory *m, size_t entity_id, Camera *component) {
	if (!check_entity_component(m, entity_id, CameraComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pCameras->count; i++) {
		if (m->components->pCameras->entity_ids[i] == entity_id) {
			*component = m->components->pCameras->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(Memory *m, size_t entity_id, Model *component) {
	if (!check_entity_component(m, entity_id, ModelComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pModels->count; i++) {
		if (m->components->pModels->entity_ids[i] == entity_id) {
			*component = m->components->pModels->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(Memory *m, size_t entity_id, Material *component) {
	if (!check_entity_component(m, entity_id, MaterialComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pMaterials->count; i++) {
		if (m->components->pMaterials->entity_ids[i] == entity_id) {
			*component = m->components->pMaterials->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(Memory *m, size_t entity_id, Input *component) {
	if (!check_entity_component(m, entity_id, InputComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pInputs->count; i++) {
		if (m->components->pInputs->entity_ids[i] == entity_id) {
			*component = m->components->pInputs->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(Memory *m, size_t entity_id, Velocity *component) {
	if (!check_entity_component(m, entity_id, VelocityComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pVelocitys->count; i++) {
		if (m->components->pVelocitys->entity_ids[i] == entity_id) {
			*component = m->components->pVelocitys->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(Memory *m, size_t entity_id, ForceAccumulator *component) {
	if (!check_entity_component(m, entity_id, ForceAccumulatorComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pForceAccumulators->count; i++) {
		if (m->components->pForceAccumulators->entity_ids[i] == entity_id) {
			*component = m->components->pForceAccumulators->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(Memory *m, size_t entity_id, RigidBody *component) {
	if (!check_entity_component(m, entity_id, RigidBodyComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pRigidBodys->count; i++) {
		if (m->components->pRigidBodys->entity_ids[i] == entity_id) {
			*component = m->components->pRigidBodys->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(Memory *m, size_t entity_id, Collider *component) {
	if (!check_entity_component(m, entity_id, ColliderComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pColliders->count; i++) {
		if (m->components->pColliders->entity_ids[i] == entity_id) {
			*component = m->components->pColliders->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(Memory *m, size_t entity_id, Texture *component) {
	if (!check_entity_component(m, entity_id, TextureComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pTextures->count; i++) {
		if (m->components->pTextures->entity_ids[i] == entity_id) {
			*component = m->components->pTextures->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(Memory *m, size_t entity_id, Mass *component) {
	if (!check_entity_component(m, entity_id, MassComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pMasss->count; i++) {
		if (m->components->pMasss->entity_ids[i] == entity_id) {
			*component = m->components->pMasss->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(Memory *m, size_t entity_id, Gravity *component) {
	if (!check_entity_component(m, entity_id, GravityComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pGravitys->count; i++) {
		if (m->components->pGravitys->entity_ids[i] == entity_id) {
			*component = m->components->pGravitys->components[i];
			return true;
		}
	}
	return false;
}

bool set_component(Memory *m, size_t entity_id, Position component) {
	if (!check_entity_component(m, entity_id, PositionComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pPositions->count; i++) {
		if (m->components->pPositions->entity_ids[i] == entity_id) {
			m->components->pPositions->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(Memory *m, size_t entity_id, Rotation component) {
	if (!check_entity_component(m, entity_id, RotationComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pRotations->count; i++) {
		if (m->components->pRotations->entity_ids[i] == entity_id) {
			m->components->pRotations->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(Memory *m, size_t entity_id, Color component) {
	if (!check_entity_component(m, entity_id, ColorComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pColors->count; i++) {
		if (m->components->pColors->entity_ids[i] == entity_id) {
			m->components->pColors->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(Memory *m, size_t entity_id, Camera component) {
	if (!check_entity_component(m, entity_id, CameraComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pCameras->count; i++) {
		if (m->components->pCameras->entity_ids[i] == entity_id) {
			m->components->pCameras->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(Memory *m, size_t entity_id, Model component) {
	if (!check_entity_component(m, entity_id, ModelComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pModels->count; i++) {
		if (m->components->pModels->entity_ids[i] == entity_id) {
			m->components->pModels->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(Memory *m, size_t entity_id, Material component) {
	if (!check_entity_component(m, entity_id, MaterialComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pMaterials->count; i++) {
		if (m->components->pMaterials->entity_ids[i] == entity_id) {
			m->components->pMaterials->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(Memory *m, size_t entity_id, Input component) {
	if (!check_entity_component(m, entity_id, InputComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pInputs->count; i++) {
		if (m->components->pInputs->entity_ids[i] == entity_id) {
			m->components->pInputs->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(Memory *m, size_t entity_id, Velocity component) {
	if (!check_entity_component(m, entity_id, VelocityComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pVelocitys->count; i++) {
		if (m->components->pVelocitys->entity_ids[i] == entity_id) {
			m->components->pVelocitys->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(Memory *m, size_t entity_id, ForceAccumulator component) {
	if (!check_entity_component(m, entity_id, ForceAccumulatorComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pForceAccumulators->count; i++) {
		if (m->components->pForceAccumulators->entity_ids[i] == entity_id) {
			m->components->pForceAccumulators->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(Memory *m, size_t entity_id, RigidBody component) {
	if (!check_entity_component(m, entity_id, RigidBodyComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pRigidBodys->count; i++) {
		if (m->components->pRigidBodys->entity_ids[i] == entity_id) {
			m->components->pRigidBodys->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(Memory *m, size_t entity_id, Collider component) {
	if (!check_entity_component(m, entity_id, ColliderComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pColliders->count; i++) {
		if (m->components->pColliders->entity_ids[i] == entity_id) {
			m->components->pColliders->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(Memory *m, size_t entity_id, Texture component) {
	if (!check_entity_component(m, entity_id, TextureComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pTextures->count; i++) {
		if (m->components->pTextures->entity_ids[i] == entity_id) {
			m->components->pTextures->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(Memory *m, size_t entity_id, Mass component) {
	if (!check_entity_component(m, entity_id, MassComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pMasss->count; i++) {
		if (m->components->pMasss->entity_ids[i] == entity_id) {
			m->components->pMasss->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(Memory *m, size_t entity_id, Gravity component) {
	if (!check_entity_component(m, entity_id, GravityComponent)) {
		return false;
	}
	for (size_t i = 0; i < m->components->pGravitys->count; i++) {
		if (m->components->pGravitys->entity_ids[i] == entity_id) {
			m->components->pGravitys->components[i] = component;
			return true;
		}
	}
	return false;
}

void ecs_load_level(game *g, const char *sceneFilePath) {
	FILE *fp;
	char errbuf[200];

	fopen_s(&fp,sceneFilePath, "r");
	if (!fp) {
		strerror_s(errbuf, sizeof(errbuf), errno);
		printf("Cannot open %s - %s\n", sceneFilePath, errbuf);
		return;
	}
	toml_table_t *level = toml_parse_file(fp, errbuf, sizeof(errbuf));
	fclose(fp);
	World *w = get_world(g);
	Memory *m = get_header(g);
	for (int i = 0;; i++) {
		const char *friendly_name = toml_key_in(level, i);
		if (!friendly_name)
			break;
		size_t entity = create_entity(w);
		set_entity_name(w, entity, friendly_name);
		toml_table_t *attributes = toml_table_in(level, friendly_name);
		if (!attributes)
			return;
		for (int j = 0;; j++) {
			const char *type_key = toml_key_in(attributes, j);
			if (!type_key)
				break;
			toml_table_t *nt = toml_table_in(attributes, type_key);
			if (!nt)
				return;
			switch (mapStringToComponentType(type_key)) {
			case PositionType: {
				Position c {
					 .x = static_cast<float>(toml_double_in(nt, "x").u.d),
					 .y = static_cast<float>(toml_double_in(nt, "y").u.d),
					 .z = static_cast<float>(toml_double_in(nt, "z").u.d),
				};
				add_component(m,entity,c);
				break;
			}
			case RotationType: {
				Rotation c {
					 .x = static_cast<float>(toml_double_in(nt, "x").u.d),
					 .y = static_cast<float>(toml_double_in(nt, "y").u.d),
					 .z = static_cast<float>(toml_double_in(nt, "z").u.d),
					 .w = static_cast<float>(toml_double_in(nt, "w").u.d),
				};
				add_component(m,entity,c);
				break;
			}
			case ColorType: {
				Color c {
					 .x = static_cast<float>(toml_double_in(nt, "x").u.d),
					 .y = static_cast<float>(toml_double_in(nt, "y").u.d),
					 .z = static_cast<float>(toml_double_in(nt, "z").u.d),
				};
				add_component(m,entity,c);
				break;
			}
			case CameraType: {
				Camera c {
					 .fov = static_cast<float>(toml_double_in(nt, "fov").u.d),
					 .near = static_cast<float>(toml_double_in(nt, "near").u.d),
					 .far = static_cast<float>(toml_double_in(nt, "far").u.d),
				};
				add_component(m,entity,c);
				break;
			}
			case ModelType: {
				Model c {
					 .shader = toml_string_in(nt, "shader").u.s
				};
				load_shader(m,entity,&c);
				break;
			}
			case MaterialType: {
				Material c {
					 .shader_id = static_cast<GLuint>(toml_int_in(nt, "shader_id").u.i),
				};
				add_component(m,entity,c);
				break;
			}
			case InputType: {
				Input c {
					 .x = static_cast<float>(toml_double_in(nt, "x").u.d),
					 .y = static_cast<float>(toml_double_in(nt, "y").u.d),
					 .z = static_cast<float>(toml_double_in(nt, "z").u.d),
				};
				add_component(m,entity,c);
				break;
			}
			case VelocityType: {
					 toml_table_t* vec_linear = toml_table_in(nt,"linear");
					 toml_table_t* vec_angular = toml_table_in(nt,"angular");
				Velocity c {
					 .linear = glm::vec3(
							static_cast<float>(toml_double_in(vec_linear,"x").u.d),
							static_cast<float>(toml_double_in(vec_linear,"y").u.d),
							static_cast<float>(toml_double_in(vec_linear,"z").u.d)),
					 .angular = glm::vec3(
							static_cast<float>(toml_double_in(vec_angular,"x").u.d),
							static_cast<float>(toml_double_in(vec_angular,"y").u.d),
							static_cast<float>(toml_double_in(vec_angular,"z").u.d)),
				};
				add_component(m,entity,c);
				break;
			}
			case ForceAccumulatorType: {
					 toml_table_t* vec_force = toml_table_in(nt,"force");
					 toml_table_t* vec_torque = toml_table_in(nt,"torque");
				ForceAccumulator c {
					 .force = glm::vec3(
							static_cast<float>(toml_double_in(vec_force,"x").u.d),
							static_cast<float>(toml_double_in(vec_force,"y").u.d),
							static_cast<float>(toml_double_in(vec_force,"z").u.d)),
					 .torque = glm::vec3(
							static_cast<float>(toml_double_in(vec_torque,"x").u.d),
							static_cast<float>(toml_double_in(vec_torque,"y").u.d),
							static_cast<float>(toml_double_in(vec_torque,"z").u.d)),
				};
				add_component(m,entity,c);
				break;
			}
			case RigidBodyType: {
				RigidBody c {
					 .linearDamping = static_cast<float>(toml_double_in(nt, "linearDamping").u.d),
					 .angularDamping = static_cast<float>(toml_double_in(nt, "angularDamping").u.d),
				};
				add_component(m,entity,c);
				break;
			}
			case ColliderType: {
				Collider c {
					 .x = static_cast<float>(toml_double_in(nt, "x").u.d),
					 .y = static_cast<float>(toml_double_in(nt, "y").u.d),
					 .z = static_cast<float>(toml_double_in(nt, "z").u.d),
				};
				add_component(m,entity,c);
				break;
			}
			case TextureType: {
				Texture c {
					 .x = static_cast<float>(toml_double_in(nt, "x").u.d),
					 .y = static_cast<float>(toml_double_in(nt, "y").u.d),
					 .z = static_cast<float>(toml_double_in(nt, "z").u.d),
				};
				add_component(m,entity,c);
				break;
			}
			case MassType: {
				Mass c {
					 .inv = static_cast<float>(toml_double_in(nt, "inv").u.d),
				};
				add_component(m,entity,c);
				break;
			}
			case GravityType: {
				Gravity c {
					 .value = static_cast<float>(toml_double_in(nt, "value").u.d),
				};
				add_component(m,entity,c);
				break;
			}
			case UNKNOWN_TYPE: {
				printf("UNKNOWN_TYPE: %s\n",type_key);
				break;
			}
			}
		}
	}
}
void save_level(Memory *m, const char *saveFilePath) {
	FILE *fp;
	fopen_s(&fp,saveFilePath, "w");
	if (!fp) {
		printf("Failed to open file %s for writing.\n", saveFilePath);
		return;
	}

	World *w = &m->world;
	for (size_t i = 0; i < w->entity_count; ++i) {
		size_t entity_id = w->entity_ids[i];
		uint32_t mask = w->component_masks[entity_id];
		if(mask & PositionComponent) {
			Position position;
			if (get_component(m, entity_id, &position)) {
				fprintf(fp,"position = { x = %.2f, y = %.2f, z = %.2f }", position.x, position.y, position.z);
			}
		}
		if(mask & RotationComponent) {
			Rotation rotation;
			if (get_component(m, entity_id, &rotation)) {
				fprintf(fp,"rotation = { x = %.2f, y = %.2f, z = %.2f, w = %.2f }", rotation.x, rotation.y, rotation.z, rotation.w);
			}
		}
		if(mask & ColorComponent) {
			Color color;
			if (get_component(m, entity_id, &color)) {
				fprintf(fp,"color = { x = %.2f, y = %.2f, z = %.2f }", color.x, color.y, color.z);
			}
		}
		if(mask & CameraComponent) {
			Camera camera;
			if (get_component(m, entity_id, &camera)) {
				fprintf(fp,"camera = { fov = %.2f, near = %.2f, far = %.2f }", camera.fov, camera.near, camera.far);
			}
		}
		if(mask & ModelComponent) {
			Model model;
			if (get_component(m, entity_id, &model)) {
				fprintf(fp,"model = { shader = %s }\n", model.shader);
			}
		}
		if(mask & MaterialComponent) {
			Material material;
			if (get_component(m, entity_id, &material)) {
				fprintf(fp,"material = { shader_id = %u }", material.shader_id);
			}
		}
		if(mask & InputComponent) {
			Input input;
			if (get_component(m, entity_id, &input)) {
				fprintf(fp,"input = { x = %.2f, y = %.2f, z = %.2f }", input.x, input.y, input.z);
			}
		}
		if(mask & VelocityComponent) {
			Velocity velocity;
			if (get_component(m, entity_id, &velocity)) {
				fprintf(fp,"velocity = { linear = { x = %.2f, y= %.2f, z= %.2f }, angular = { x = %.2f, y= %.2f, z= %.2f } }", velocity.linear.x, velocity.linear.y, velocity.linear.z, velocity.angular.x, velocity.angular.y, velocity.angular.z);
			}
		}
		if(mask & ForceAccumulatorComponent) {
			ForceAccumulator forceAccumulator;
			if (get_component(m, entity_id, &forceAccumulator)) {
				fprintf(fp,"forceAccumulator = { force = { x = %.2f, y= %.2f, z= %.2f }, torque = { x = %.2f, y= %.2f, z= %.2f } }", forceAccumulator.force.x, forceAccumulator.force.y, forceAccumulator.force.z, forceAccumulator.torque.x, forceAccumulator.torque.y, forceAccumulator.torque.z);
			}
		}
		if(mask & RigidBodyComponent) {
			RigidBody rigidBody;
			if (get_component(m, entity_id, &rigidBody)) {
				fprintf(fp,"rigidBody = { linearDamping = %.2f, angularDamping = %.2f }", rigidBody.linearDamping, rigidBody.angularDamping);
			}
		}
		if(mask & ColliderComponent) {
			Collider collider;
			if (get_component(m, entity_id, &collider)) {
				fprintf(fp,"collider = { x = %.2f, y = %.2f, z = %.2f }", collider.x, collider.y, collider.z);
			}
		}
		if(mask & TextureComponent) {
			Texture texture;
			if (get_component(m, entity_id, &texture)) {
				fprintf(fp,"texture = { x = %.2f, y = %.2f, z = %.2f }", texture.x, texture.y, texture.z);
			}
		}
		if(mask & MassComponent) {
			Mass mass;
			if (get_component(m, entity_id, &mass)) {
				fprintf(fp,"mass = { inv = %.2f }", mass.inv);
			}
		}
		if(mask & GravityComponent) {
			Gravity gravity;
			if (get_component(m, entity_id, &gravity)) {
				fprintf(fp,"gravity = { value = %.2f }", gravity.value);
			}
		}
		fprintf(fp, "\n");
	}
	fclose(fp);
	printf("World saved to %s\n", saveFilePath);
}
