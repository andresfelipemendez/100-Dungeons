#include "ecs.h"
#include "ext/matrix_clip_space.hpp"
#include "ext/matrix_float4x4.hpp"
#include "ext/matrix_transform.hpp"
#include "ext/vector_float3.hpp"
#include "fwd.hpp"
#include "memory.h"
#include <assert.h>

#include <cctype>

#include <cstddef>
#include <cstring>
#include <game.h>
#include <stdio.h>
#include <string.h>
#include <toml.h>

#include "asset_loader.h"
#include "trigonometric.hpp"

#include <glm.hpp>
#include <gtc/constants.hpp>
#include <gtc/matrix_transform.hpp>

#ifdef _WIN32
#define strcasecmp _stricmp
#endif

const char *component_names[] = {
#define X(name) #name,
	SUBKEY_TYPES
#undef X
};

extern size_t component_count;
size_t component_count = sizeof(component_names) / sizeof(component_names[0]);

SubkeyType mapStringToSubkeyType(const char *type_key) {
#define X(name)                                                                \
	if (strcasecmp(type_key, #name) == 0)                                      \
		return name##_TYPE;
	SUBKEY_TYPES
#undef X
	return UNKNOWN_TYPE;
}

bool get_entity_name(World *w, size_t entity, char *name) {
	for (size_t i = 0; i < w->entity_count; ++i) {
		if (w->entity_ids[i] == entity) {
			size_t name_length = strlen(w->entity_names[i]);
			const char *entity_name = w->entity_names[i];
			strcpy_s(name, ENTITY_NAME_LENGTH, entity_name);
			name[name_length] = '\0';
			return true;
		}
	}
	return false;
}

bool check_entity_component(MemoryHeader *h, size_t entity,
							uint32_t component_mask) {

	if (!(h->world.component_masks[entity] & component_mask)) {
		char entity_name[ENTITY_NAME_LENGTH]{0};
		get_entity_name(&h->world, entity, entity_name);

		int component_index = -1;
		for (size_t i = 0; i < component_count; ++i) {
			if ((component_mask >> i) & 1) {
				component_index = i;
				break;
			}
		}
		const char *component_name = (component_index != -1)
										 ? component_names[component_index]
										 : "UNKNOWN";
		printf("entity %s does not have component %s", entity_name,
			   component_name);
		return false;
	}
	return true;
}

size_t create_entity(World *w) {
	size_t entity_id = w->entity_count;
	w->entity_ids[entity_id] = entity_id;
	snprintf(w->entity_names[entity_id], ENTITY_NAME_LENGTH, "Entity %zu",
			 entity_id);
	w->component_masks[entity_id] = 0;
	w->entity_count++;
	return entity_id;
}

bool get_entity(MemoryHeader *h, uint32_t component_mask,
				size_t &out_entity_id) {
	for (size_t i = 0; i < h->world.entity_count; ++i) {
		if (h->world.component_masks[i] & component_mask) {
			out_entity_id = h->world.entity_ids[i];
			return true;
		}
	}
	return false;
}

bool get_entities(MemoryHeader *h, uint32_t component_mask) {
	h->query.count = 0;
	for (size_t i = 0; i < h->world.entity_count; ++i) {
		if ((h->world.component_masks[i] & component_mask) == component_mask) {
			h->query.entities[h->query.count] = h->world.entity_ids[i];
			h->query.count++;
		}
	}
	return true;
}

void set_entity_name(World *w, size_t entity, const char *friendly_name) {
	size_t name_length = strlen(friendly_name);
	if (name_length > ENTITY_NAME_LENGTH) {
		printf("name : %s, it's too long", friendly_name);
		return;
	}
	memset(w->entity_names[entity], '\0', ENTITY_NAME_LENGTH);
	strncpy_s(w->entity_names[entity], friendly_name, name_length);
	w->entity_names[entity][name_length] = '\0';
}

void add_component(MemoryHeader *h, size_t entity_id, uint32_t component_mask) {
	switch (component_mask) {
	case COMPONENT_CAMERA: {
		size_t i = h->cameras->count;
		h->cameras->entity_ids[i] = entity_id;
		h->cameras->count++;
		break;
	}
	case COMPONENT_POSITION: {
		size_t i = h->transforms->count;
		h->transforms->entity_ids[i] = entity_id;
		h->transforms->count++;
		break;
	}
	case COMPONENT_MATERIAL: {
		size_t i = h->materials->count;
		h->materials->entity_ids[i] = entity_id;
		h->materials->count++;
		break;
	}
	}
	h->world.component_masks[entity_id] |= component_mask;
}

void add_component(MemoryHeader *h, size_t entity, Camera camera) {
	char entity_name[ENTITY_NAME_LENGTH];
	get_entity_name(&h->world, entity, entity_name);
	printf("add camera component to entity %s", entity_name);
	size_t i = h->cameras->count;
	h->cameras->entity_ids[i] = entity;
	h->cameras->cameras[i] = camera;
	h->cameras->count++;
	h->world.component_masks[entity] |= COMPONENT_CAMERA;
}

bool add_shader(MemoryHeader *h, char *name, GLuint programID) {

	size_t name_length = strlen(name);
	if (name_length > ENTITY_NAME_LENGTH) {
		printf("shader name should be less than %i, name: %s \n",
			   ENTITY_NAME_LENGTH, name);
	}

	for (size_t i = 0; i < name_length; ++i) {
		name[i] = tolower((unsigned char)name[i]);
	}

	errno_t err = strncpy_s(h->shaders->shader_names[h->shaders->count],
							ENTITY_NAME_LENGTH, name, name_length);
	if (err != 0) {
		printf("error copying shader name %s\n", name);
		return false;
	}

	h->shaders->shader_names[h->shaders->count][name_length] = '\0';
	h->shaders->program_ids[h->shaders->count] = programID;
	h->shaders->count++;

	return true;
}

bool get_shader_by_name_caseinsenstive(MemoryHeader *h, const char *name,
									   GLuint *programID) {
	size_t shader_name_length = strlen(name);
	if (shader_name_length >= ENTITY_NAME_LENGTH) {
		printf("material name it's too long %s", name);
		return false;
	}

	char material_name[ENTITY_NAME_LENGTH];
	strcpy_s(material_name, name);
	for (size_t i = 0; i < ENTITY_NAME_LENGTH; ++i) {
		material_name[i] = tolower((unsigned char)material_name[i]);
	}

	for (size_t i = 0; i < h->shaders->count; ++i) {
		if (strcmp(material_name, h->shaders->shader_names[i])) {
			*programID = h->shaders->program_ids[i];
			return true;
		}
	}

	return false;
}

bool get_component_value(MemoryHeader *h, size_t entity, Vec3 *value) {
	if (!check_entity_component(h, entity, COMPONENT_POSITION))
		return false;

	for (size_t i = 0; i < h->transforms->count; i++) {
		if (h->transforms->entity_ids[i] == entity) {
			*value = h->transforms->positions[i];
			return true;
		}
	}

	return false;
}

bool set_component_value(MemoryHeader *h, size_t entity, Vec3 value) {
	if (!check_entity_component(h, entity, COMPONENT_POSITION))
		return false;

	for (size_t i = 0; i < h->transforms->count; i++) {
		if (h->transforms->entity_ids[i] == entity) {
			h->transforms->positions[i] = value;
			return true;
		}
	}
	return false;
}

bool get_component_value(MemoryHeader *h, size_t entity, Camera *value) {
	if (!check_entity_component(h, entity, COMPONENT_CAMERA))
		return false;

	for (size_t i = 0; i < h->cameras->count; i++) {
		if (h->cameras->entity_ids[i] == entity) {
			*value = h->cameras->cameras[i];
			return true;
		}
	}

	return false;
}

bool set_component_value(MemoryHeader *h, size_t entity, Camera value) {
	if (!check_entity_component(h, entity, COMPONENT_CAMERA))
		return false;

	for (size_t i = 0; i < h->cameras->count; i++) {
		if (h->cameras->entity_ids[i] == entity) {
			h->cameras->cameras[i] = value;
			return true;
		}
	}

	return false;
}

bool get_component_value(MemoryHeader *h, size_t entity, StaticMesh *value) {
	if (!check_entity_component(h, entity, COMPONENT_MODEL))
		return false;

	for (size_t i = 0; i < h->meshes->count; i++) {
		if (h->meshes->entity_ids[i] == entity) {
			*value = h->meshes->mesh_data[i];
			return true;
		}
	}

	return false;
}

bool set_component_value(MemoryHeader *h, size_t entity, StaticMesh value) {
	if (!check_entity_component(h, entity, COMPONENT_MODEL))
		return false;

	for (size_t i = 0; i < h->meshes->count; i++) {
		if (h->meshes->entity_ids[i] == entity) {
			h->meshes->mesh_data[i] = value;
			return true;
		}
	}

	return false;
}

bool get_component_value(MemoryHeader *h, size_t entity, Material *value) {
	if (!check_entity_component(h, entity, COMPONENT_MATERIAL))
		return false;

	for (size_t i = 0; i < h->materials->count; i++) {
		if (h->materials->entity_ids[i] == entity) {
			*value = h->materials->materials[i];
			return true;
		}
	}
	return false;
}
bool set_component_value(MemoryHeader *h, size_t entity, Material value) {
	if (!check_entity_component(h, entity, COMPONENT_MATERIAL))
		return false;

	for (size_t i = 0; i < h->materials->count; i++) {
		if (h->materials->entity_ids[i] == entity) {
			h->materials->materials[i] = value;
			return true;
		}
	}

	return false;
}

void load_material(MemoryHeader *h, size_t entity, const char *material_name) {

	GLuint shader_id;
	if (!get_shader_by_name_caseinsenstive(h, material_name, &shader_id)) {
		printf("can't find shader by case insensitive name %s\n",
			   material_name);
	}
	add_component(h, entity, COMPONENT_MATERIAL);
	if (!set_component_value(h, entity, Material{.shader_id = 0})) {
		printf("error setting component material value\n");
	}
}

void ecs_load_level(game *g, const char *sceneFilePath) {
	FILE *fp;
	char errbuf[200];

	fp = fopen(sceneFilePath, "r");
	if (!fp) {
		printf("Cannot open %s - %s\n", sceneFilePath, strerror(errno));
		return;
	}

	toml_table_t *level = toml_parse_file(fp, errbuf, sizeof(errbuf));
	fclose(fp);

	World *w = get_world(g);
	MemoryHeader *h = get_header(g);

	for (int i = 0;; i++) {
		const char *friendly_name = toml_key_in(level, i);
		if (!friendly_name)
			break;

		size_t entity = create_entity(w);
		set_entity_name(w, entity, friendly_name);

		toml_table_t *attributes = toml_table_in(level, friendly_name);
		if (attributes) {
			// printf("[%s]\n", friendly_name);

			for (int j = 0;; j++) {
				const char *type_key = toml_key_in(attributes, j);
				if (!type_key)
					break;

				switch (mapStringToSubkeyType(type_key)) {
				case POSITION_TYPE: {
					toml_table_t *nested_table =
						toml_table_in(attributes, type_key);
					if (nested_table) {
						toml_datum_t x_datum =
							toml_double_in(nested_table, "x");
						toml_datum_t y_datum =
							toml_double_in(nested_table, "y");
						toml_datum_t z_datum =
							toml_double_in(nested_table, "z");

						float x = static_cast<float>(x_datum.u.d);
						float y = static_cast<float>(y_datum.u.d);
						float z = static_cast<float>(z_datum.u.d);

						// printf("  %s = { x = %.2f, y = %.2f, z = %.2f }\n",
						//	   type_key, x, y, z);

						add_component(h, entity, COMPONENT_POSITION);
						if (set_component_value(h, entity, Vec3{x, y, z})) {
							// printf("set position\n");
						}
					}
					break;
				}
				case ROTATION_TYPE: {
					toml_table_t *nested_table =
						toml_table_in(attributes, type_key);
					if (nested_table) {
						toml_datum_t x = toml_double_in(nested_table, "pitch");
						toml_datum_t y = toml_double_in(nested_table, "yaw");
						toml_datum_t z = toml_double_in(nested_table, "roll");

						// printf("  %s = { x = %.2f, y = %.2f, z = %.2f }\n",
						// 	   type_key, x.u.d, y.u.d, z.u.d);
					}
					break;
				}
				case COLOR_TYPE: {
					toml_table_t *nested_table =
						toml_table_in(attributes, type_key);
					if (nested_table) {
						toml_datum_t r = toml_double_in(nested_table, "r");
						toml_datum_t g = toml_double_in(nested_table, "g");
						toml_datum_t b = toml_double_in(nested_table, "b");

						printf("  %s = { r = %.2f, g = %.2f, b = %.2f }\n",
							   type_key, r.u.d, g.u.d, b.u.d);
					}
					break;
				}
				case CAMERA_TYPE: {
					toml_table_t *nested_table =
						toml_table_in(attributes, type_key);
					if (nested_table) {
						toml_datum_t fov_datum =
							toml_double_in(nested_table, "fov");
						toml_datum_t near_datum =
							toml_double_in(nested_table, "near");
						toml_datum_t far_datum =
							toml_double_in(nested_table, "far");

						float fov = static_cast<float>(fov_datum.u.d);
						float near = static_cast<float>(near_datum.u.d);
						float far = static_cast<float>(far_datum.u.d);

						// printf(
						// 	"  %s = { fov = %.2f, near = %.2f, far = %.2f }\n",
						// 	type_key, fov, near, far);

						glm::vec3 cameraPosition = glm::vec3(-1.5f, 1.0f, 2.0f);
						glm::vec3 targetPosition = glm::vec3(1.0f, 0.0f, 0.0f);
						glm::vec3 upDirection = glm::vec3(0.0f, 0.0f, 1.0f);

						glm::mat4 view = glm::lookAt(
							cameraPosition, targetPosition, upDirection);

						float aspectRatio = 16.0f / 9.0f;

						glm::mat4 projection = glm::perspective(
							glm::radians(fov), aspectRatio, near, far);

						glm::mat4 viewProj = projection * view;
						Camera camera{.projection = viewProj};
						add_component(h, entity, camera);

						// printf("set camera projection");
					}
					break;
				}
				case MODEL_TYPE: {
					toml_datum_t model = toml_string_in(attributes, type_key);

					if (model.ok) {
						// printf("  model = \"%s\"\n", model.u.s);
						StaticMesh staticMesh;
						if (!LoadGLTFMeshes(h, model.u.s, &staticMesh)) {
							printf("error loading model to ecs\n");
						}

						add_component(h, entity, COMPONENT_MODEL);
						if (set_component_value(h, entity, staticMesh)) {
						}
						free(model.u.s);
					}
					break;
				}
				case MATERIAL_TYPE: {
					toml_datum_t material =
						toml_string_in(attributes, type_key);

					if (material.ok) {
						load_material(h, entity, material.u.s);
					}
					break;
				}
				case TEXTURE_TYPE: {
					break;
				}
				case FOV_TYPE: {
					toml_datum_t fov_value =
						toml_double_in(attributes, type_key);
					if (fov_value.ok)
						printf("  %s = %.2f\n", type_key, fov_value.u.d);
					break;
				}
				case INTENSITY_TYPE: {
					toml_datum_t intensity_value =
						toml_double_in(attributes, type_key);
					if (intensity_value.ok)
						printf("  %s = %.2f\n", type_key, intensity_value.u.d);
					break;
				}
				default:
					printf("  %s = (unknown type)\n", type_key);
				}
			}
			printf("\n");
		}
	}

	toml_free(level);
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

		// Write the entity's name as the TOML table header
		fprintf(fp, "[%s]\n", w->entity_names[entity_id]);

		// Serialize each component type based on the component mask
		if (mask & COMPONENT_POSITION) {
			Vec3 position;
			if (get_component_value(h, entity_id, &position)) {
				fprintf(fp, "position = { x = %.2f, y = %.2f, z = %.2f }\n",
						position.x, position.y, position.z);
			}
		}

		// if (mask & COMPONENT_ROTATION) {
		// 	Vec3 rotation;
		// 	if (get_component_value(h, entity_id, COMPONENT_ROTATION,
		// &rotation)) { 		fprintf(fp, "rotation = { pitch = %.2f, yaw =
		// %.2f, roll = %.2f
		// }\n", rotation.x, rotation.y, rotation.z);
		// 	}
		// }

		// if (mask & COMPONENT_COLOR) {
		// 	Vec3 color;
		// 	if (get_component_value(h, entity_id, COMPONENT_COLOR, &color)) {
		// 		fprintf(fp, "color = { r = %.2f, g = %.2f, b = %.2f }\n",
		// color.x, color.y, color.z);
		// 	}
		// }

		// if (mask & COMPONENT_FOV) {
		// 	double fov;
		// 	if (get_component_value(h, entity_id, COMPONENT_FOV, &fov)) {
		// 		fprintf(fp, "fov = %.2f\n", fov);
		// 	}
		// }

		// if (mask & COMPONENT_INTENSITY) {
		// 	double intensity;
		// 	if (get_component_value(h, entity_id, COMPONENT_INTENSITY,
		// &intensity)) { 		fprintf(fp, "intensity = %.2f\n", intensity);
		// 	}
		// }

		fprintf(fp, "\n"); // Add a newline between entities for readability
	}

	fclose(fp);
	printf("World saved to %s\n", saveFilePath);
}