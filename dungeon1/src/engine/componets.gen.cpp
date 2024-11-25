#include "components.gen.h"
#include "ecs.h"

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
