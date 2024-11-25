#include "ecs.h"
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

#include <glm.hpp>
#include <gtc/constants.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>

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
		return name##Type;
	SUBKEY_TYPES
#undef X
	return UNKNOWN_TYPE;
}

#define DEFINE_ADD_COMPONENT_FUNCTION(name)                                    \
	void add_component(MemoryHeader *h, size_t entity_id, name component) {    \
		size_t i = h->p##name##s->count;                                       \
		h->p##name##s->entity_ids[i] = entity_id;                              \
		h->p##name##s->components[i] = component;                              \
		h->p##name##s->count++;                                                \
		h->world.component_masks[entity_id] |= name##Component;                \
	}

#define X(name) DEFINE_ADD_COMPONENT_FUNCTION(name)
SUBKEY_TYPES
#undef X

#undef DEFINE_ADD_COMPONENT_FUNCTION

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
		if ((h->world.component_masks[i] & component_mask) == component_mask) {
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
		if (strcmp(material_name, h->shaders->shader_names[i]) == 0) {
			*programID = h->shaders->program_ids[i];
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
	Material m{.shader_id = shader_id};
	add_component(h, entity, m);
}

void load_collider(MemoryHeader *h, size_t entity, toml_datum_t *collider) {

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
				case PositionType: {
					toml_table_t *nt = toml_table_in(attributes, type_key);
					if (nt) {
						toml_datum_t x_datum = toml_double_in(nt, "x");
						toml_datum_t y_datum = toml_double_in(nt, "y");
						toml_datum_t z_datum = toml_double_in(nt, "z");

						float x = static_cast<float>(x_datum.u.d);
						float y = static_cast<float>(y_datum.u.d);
						float z = static_cast<float>(z_datum.u.d);

						printf("%s = { x = %.2f, y = %.2f, z = %.2f}\n",
							   type_key, x, y, z);
						Position p{x, y, z};
						add_component(h, entity, p);
					}
					break;
				}
				case RotationType: {
					toml_table_t *nested_table =
						toml_table_in(attributes, type_key);
					if (nested_table) {
						toml_datum_t p_datum =
							toml_double_in(nested_table, "pitch");
						toml_datum_t y_datum =
							toml_double_in(nested_table, "yaw");
						toml_datum_t r_datum =
							toml_double_in(nested_table, "roll");

						float p = glm::radians(static_cast<float>(p_datum.u.d));
						float y = glm::radians(static_cast<float>(y_datum.u.d));
						float r = glm::radians(static_cast<float>(r_datum.u.d));

						glm::quat q = glm::quat(glm::vec3(p, y, r));

						Rotation rot{.x = q.x, .y = q.y, .z = q.z, .w = q.w};

						add_component(h, entity, rot);

						printf("  %s = { p = %.2f, y = %.2f, r = %.2f }\n",
							   type_key, p, y, r);
					}
					break;
				}
				case ColorType: {
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
				case CameraType: {
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

						printf(
							"  %s = { fov = %.2f, near = %.2f, far = %.2f }\n",
							type_key, fov, near, far);

						Camera camera{fov, near, far};
						add_component(h, entity, camera);
					}
					break;
				}
				case ModelType: {
					toml_datum_t model = toml_string_in(attributes, type_key);

					if (model.ok) {
						printf("  model = \"%s\"\n", model.u.s);
						Model* m = &h->pModels->components[h->pModels->count];
						g->load_mesh(g, model.u.s);
						add_component(h, entity, *m);
						free(model.u.s);
					}
					break;
				}
				case MaterialType: {
					toml_datum_t material =
						toml_string_in(attributes, type_key);

					if (material.ok) {
						load_material(h, entity, material.u.s);
					}
					break;
				}
				case InputType: {
					toml_datum_t input = toml_string_in(attributes, type_key);
					if (input.ok) {
						Input i{};
						add_component(h, entity, i);
						printf("input\n");
					}
					break;
				}
			case ColliderType:{
				toml_datum_t collider = toml_string_in(attributes, type_key);
				if(collider.ok){
					load_collider(h, entity,&collider);
				}
				break;
			}
				case TextureType: {
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
