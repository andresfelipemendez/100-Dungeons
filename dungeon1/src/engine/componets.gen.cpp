#include "components.gen.h"
#include "ecs.h"

bool get_component(MemoryHeader *h, size_t entity_id, Position *component) {
	if (!check_entity_component(h, entity_id, PositionComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pPositions->count; i++) {
		if (h->pPositions->entity_ids[i] == entity_id) {
			*component = h->pPositions->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(MemoryHeader *h, size_t entity_id, Rotation *component) {
	if (!check_entity_component(h, entity_id, RotationComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pRotations->count; i++) {
		if (h->pRotations->entity_ids[i] == entity_id) {
			*component = h->pRotations->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(MemoryHeader *h, size_t entity_id, Color *component) {
	if (!check_entity_component(h, entity_id, ColorComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pColors->count; i++) {
		if (h->pColors->entity_ids[i] == entity_id) {
			*component = h->pColors->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(MemoryHeader *h, size_t entity_id, Camera *component) {
	if (!check_entity_component(h, entity_id, CameraComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pCameras->count; i++) {
		if (h->pCameras->entity_ids[i] == entity_id) {
			*component = h->pCameras->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(MemoryHeader *h, size_t entity_id, Model *component) {
	if (!check_entity_component(h, entity_id, ModelComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pModels->count; i++) {
		if (h->pModels->entity_ids[i] == entity_id) {
			*component = h->pModels->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(MemoryHeader *h, size_t entity_id, Material *component) {
	if (!check_entity_component(h, entity_id, MaterialComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pMaterials->count; i++) {
		if (h->pMaterials->entity_ids[i] == entity_id) {
			*component = h->pMaterials->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(MemoryHeader *h, size_t entity_id, Input *component) {
	if (!check_entity_component(h, entity_id, InputComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pInputs->count; i++) {
		if (h->pInputs->entity_ids[i] == entity_id) {
			*component = h->pInputs->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(MemoryHeader *h, size_t entity_id, Velocity *component) {
	if (!check_entity_component(h, entity_id, VelocityComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pVelocitys->count; i++) {
		if (h->pVelocitys->entity_ids[i] == entity_id) {
			*component = h->pVelocitys->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(MemoryHeader *h, size_t entity_id, ForceAccumulator *component) {
	if (!check_entity_component(h, entity_id, ForceAccumulatorComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pForceAccumulators->count; i++) {
		if (h->pForceAccumulators->entity_ids[i] == entity_id) {
			*component = h->pForceAccumulators->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(MemoryHeader *h, size_t entity_id, RigidBody *component) {
	if (!check_entity_component(h, entity_id, RigidBodyComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pRigidBodys->count; i++) {
		if (h->pRigidBodys->entity_ids[i] == entity_id) {
			*component = h->pRigidBodys->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(MemoryHeader *h, size_t entity_id, Collider *component) {
	if (!check_entity_component(h, entity_id, ColliderComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pColliders->count; i++) {
		if (h->pColliders->entity_ids[i] == entity_id) {
			*component = h->pColliders->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(MemoryHeader *h, size_t entity_id, Texture *component) {
	if (!check_entity_component(h, entity_id, TextureComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pTextures->count; i++) {
		if (h->pTextures->entity_ids[i] == entity_id) {
			*component = h->pTextures->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(MemoryHeader *h, size_t entity_id, Mass *component) {
	if (!check_entity_component(h, entity_id, MassComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pMasss->count; i++) {
		if (h->pMasss->entity_ids[i] == entity_id) {
			*component = h->pMasss->components[i];
			return true;
		}
	}
	return false;
}

bool get_component(MemoryHeader *h, size_t entity_id, Gravity *component) {
	if (!check_entity_component(h, entity_id, GravityComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pGravitys->count; i++) {
		if (h->pGravitys->entity_ids[i] == entity_id) {
			*component = h->pGravitys->components[i];
			return true;
		}
	}
	return false;
}

bool set_component(MemoryHeader *h, size_t entity_id, Position component) {
	if (!check_entity_component(h, entity_id, PositionComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pPositions->count; i++) {
		if (h->pPositions->entity_ids[i] == entity_id) {
			h->pPositions->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(MemoryHeader *h, size_t entity_id, Rotation component) {
	if (!check_entity_component(h, entity_id, RotationComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pRotations->count; i++) {
		if (h->pRotations->entity_ids[i] == entity_id) {
			h->pRotations->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(MemoryHeader *h, size_t entity_id, Color component) {
	if (!check_entity_component(h, entity_id, ColorComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pColors->count; i++) {
		if (h->pColors->entity_ids[i] == entity_id) {
			h->pColors->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(MemoryHeader *h, size_t entity_id, Camera component) {
	if (!check_entity_component(h, entity_id, CameraComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pCameras->count; i++) {
		if (h->pCameras->entity_ids[i] == entity_id) {
			h->pCameras->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(MemoryHeader *h, size_t entity_id, Model component) {
	if (!check_entity_component(h, entity_id, ModelComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pModels->count; i++) {
		if (h->pModels->entity_ids[i] == entity_id) {
			h->pModels->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(MemoryHeader *h, size_t entity_id, Material component) {
	if (!check_entity_component(h, entity_id, MaterialComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pMaterials->count; i++) {
		if (h->pMaterials->entity_ids[i] == entity_id) {
			h->pMaterials->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(MemoryHeader *h, size_t entity_id, Input component) {
	if (!check_entity_component(h, entity_id, InputComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pInputs->count; i++) {
		if (h->pInputs->entity_ids[i] == entity_id) {
			h->pInputs->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(MemoryHeader *h, size_t entity_id, Velocity component) {
	if (!check_entity_component(h, entity_id, VelocityComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pVelocitys->count; i++) {
		if (h->pVelocitys->entity_ids[i] == entity_id) {
			h->pVelocitys->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(MemoryHeader *h, size_t entity_id, ForceAccumulator component) {
	if (!check_entity_component(h, entity_id, ForceAccumulatorComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pForceAccumulators->count; i++) {
		if (h->pForceAccumulators->entity_ids[i] == entity_id) {
			h->pForceAccumulators->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(MemoryHeader *h, size_t entity_id, RigidBody component) {
	if (!check_entity_component(h, entity_id, RigidBodyComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pRigidBodys->count; i++) {
		if (h->pRigidBodys->entity_ids[i] == entity_id) {
			h->pRigidBodys->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(MemoryHeader *h, size_t entity_id, Collider component) {
	if (!check_entity_component(h, entity_id, ColliderComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pColliders->count; i++) {
		if (h->pColliders->entity_ids[i] == entity_id) {
			h->pColliders->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(MemoryHeader *h, size_t entity_id, Texture component) {
	if (!check_entity_component(h, entity_id, TextureComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pTextures->count; i++) {
		if (h->pTextures->entity_ids[i] == entity_id) {
			h->pTextures->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(MemoryHeader *h, size_t entity_id, Mass component) {
	if (!check_entity_component(h, entity_id, MassComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pMasss->count; i++) {
		if (h->pMasss->entity_ids[i] == entity_id) {
			h->pMasss->components[i] = component;
			return true;
		}
	}
	return false;
}

bool set_component(MemoryHeader *h, size_t entity_id, Gravity component) {
	if (!check_entity_component(h, entity_id, GravityComponent)) {
		return false
	}
	for (size_t i = 0; i < h->pGravitys->count; i++) {
		if (h->pGravitys->entity_ids[i] == entity_id) {
			h->pGravitys->components[i] = component;
			return true;
		}
	}
	return false;
}

void save_level(MemoryHeader *h, const char *saveFilePath) {
	FILE *fp = fopen(saveFilePath, "w");
	if (!fp) {
		printf("Failed to open file %s for writing.\n", saveFilePath);
		return;
	}

	World *w = &h->world;
	for (size_t i = 0; i < w->entity_count; ++i) {
	size_t entity_id = w->entity_ids[i];
	uint32_t mask = w->component_masks[entity_id];
	if(mask & PositionComponent) {
		Position position;
		if (get_component(h, entity_id, &position)) {
			fprintf(fp,"position = { x = %.2f, y = %.2f, z = %.2f }", position.x, position.y, position.z);
		}
	}
	if(mask & RotationComponent) {
		Rotation rotation;
		if (get_component(h, entity_id, &rotation)) {
			fprintf(fp,"rotation = { x = %.2f, y = %.2f, z = %.2f }", rotation.x, rotation.y, rotation.z);
		}
	}
	if(mask & ColorComponent) {
		Color color;
		if (get_component(h, entity_id, &color)) {
			fprintf(fp,"color = { x = %.2f, y = %.2f, z = %.2f }", color.x, color.y, color.z);
		}
	}
	if(mask & CameraComponent) {
		Camera camera;
		if (get_component(h, entity_id, &camera)) {
			fprintf(fp,"camera = { x = %.2f, y = %.2f, z = %.2f }", camera.x, camera.y, camera.z);
		}
	}
	if(mask & ModelComponent) {
		Model model;
		if (get_component(h, entity_id, &model)) {
			fprintf(fp,"model = { x = %.2f, y = %.2f, z = %.2f }", model.x, model.y, model.z);
		}
	}
	if(mask & MaterialComponent) {
		Material material;
		if (get_component(h, entity_id, &material)) {
			fprintf(fp,"material = { x = %.2f, y = %.2f, z = %.2f }", material.x, material.y, material.z);
		}
	}
	if(mask & InputComponent) {
		Input input;
		if (get_component(h, entity_id, &input)) {
			fprintf(fp,"input = { x = %.2f, y = %.2f, z = %.2f }", input.x, input.y, input.z);
		}
	}
	if(mask & VelocityComponent) {
		Velocity velocity;
		if (get_component(h, entity_id, &velocity)) {
			fprintf(fp,"velocity = { x = %.2f, y = %.2f, z = %.2f }", velocity.x, velocity.y, velocity.z);
		}
	}
	if(mask & ForceAccumulatorComponent) {
		ForceAccumulator forceAccumulator;
		if (get_component(h, entity_id, &forceAccumulator)) {
			fprintf(fp,"forceAccumulator = { x = %.2f, y = %.2f, z = %.2f }", forceAccumulator.x, forceAccumulator.y, forceAccumulator.z);
		}
	}
	if(mask & RigidBodyComponent) {
		RigidBody rigidBody;
		if (get_component(h, entity_id, &rigidBody)) {
			fprintf(fp,"rigidBody = { x = %.2f, y = %.2f, z = %.2f }", rigidBody.x, rigidBody.y, rigidBody.z);
		}
	}
	if(mask & ColliderComponent) {
		Collider collider;
		if (get_component(h, entity_id, &collider)) {
			fprintf(fp,"collider = { x = %.2f, y = %.2f, z = %.2f }", collider.x, collider.y, collider.z);
		}
	}
	if(mask & TextureComponent) {
		Texture texture;
		if (get_component(h, entity_id, &texture)) {
			fprintf(fp,"texture = { x = %.2f, y = %.2f, z = %.2f }", texture.x, texture.y, texture.z);
		}
	}
	if(mask & MassComponent) {
		Mass mass;
		if (get_component(h, entity_id, &mass)) {
			fprintf(fp,"mass = { x = %.2f, y = %.2f, z = %.2f }", mass.x, mass.y, mass.z);
		}
	}
	if(mask & GravityComponent) {
		Gravity gravity;
		if (get_component(h, entity_id, &gravity)) {
			fprintf(fp,"gravity = { x = %.2f, y = %.2f, z = %.2f }", gravity.x, gravity.y, gravity.z);
		}
	}
	fprintf(fp, "\n");
	}
	fclose(fp);
	printf("World saved to %s\n", saveFilePath);
}
