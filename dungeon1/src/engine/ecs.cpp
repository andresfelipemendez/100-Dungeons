#include "ecs.h"
#include <cmath>
#include <game.h>
#include "ext/matrix_clip_space.hpp"
#include "ext/matrix_float4x4.hpp"
#include "ext/matrix_transform.hpp"
#include "ext/vector_float3.hpp"
#include "memory.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <toml.h>

#include "asset_loader.h"
#include "trigonometric.hpp"

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/constants.hpp>

#ifdef _WIN32
    #define strcasecmp _stricmp
#endif

const char* component_names[] = {
    #define X(name) #name,
	SUBKEY_TYPES
    #undef X
};

extern size_t component_count;
size_t component_count =  sizeof(component_names) / sizeof(component_names[0]);

SubkeyType mapStringToSubkeyType(const char* type_key) {
    #define X(name) if (strcasecmp(type_key, #name) == 0) return name##_TYPE;
	SUBKEY_TYPES
    #undef X
	return UNKNOWN_TYPE;
}

size_t create_entity(World* w) {
	size_t entity_id = w->entity_count;
	w->entity_ids[entity_id] = entity_id;
	snprintf(w->entity_names[entity_id], ENTITY_NAME_LENGTH, "Entity %zu", entity_id);
	w->component_masks[entity_id] = 0;
	w->entity_count++;
	return entity_id;
}

void set_entity_name(World* w, size_t entity,const char* friendly_name) {
	assert(entity < w->entity_count && "Entity ID is out of bounds");
	strncpy(w->entity_names[entity], friendly_name, ENTITY_NAME_LENGTH - 1);
	w->entity_names[entity][ENTITY_NAME_LENGTH - 1] = '\0';
}

void add_component(MemoryHeader* h, size_t entity_id, uint32_t component_mask) {
	switch (component_mask)
	{
	case COMPONENT_POSITION: {
		size_t i = h->transforms->count;
		h->transforms->entity_ids[i] = entity_id;
		h->transforms->positions[i] = { 0, 0, 0 };
		h->transforms->count++;
		break;
	}
}
h->world.component_masks[entity_id] |= component_mask;
}

bool get_component_value(MemoryHeader* h, size_t entity_id, uint32_t component_mask, Vec3* value) {
	if(!(h->world.component_masks[entity_id] & component_mask)) {
		return false;
	}

	switch (component_mask)
	{
	case COMPONENT_POSITION: {
		for (size_t i = 0; i < h->transforms->count; i++) {
			if (h->transforms->entity_ids[i] == entity_id) {
				*value = h->transforms->positions[i];
				return true;
			}
		}
		break;
		}
	}

	return false;
}

bool set_component_value(MemoryHeader* h, size_t entity_id, uint32_t component_mask, Vec3 value) {
	if (!(h->world.component_masks[entity_id] & component_mask)) {
		return false;
	}

	switch (component_mask) {
	case COMPONENT_POSITION: {
		for (size_t i = 0; i < h->transforms->count; i++) {
			if (h->transforms->entity_ids[i] == entity_id) {
				h->transforms->positions[i] = value;
				return true;
			}
		}
		break;
	}
}
return false;
}

void ecs_load_level(game* g, const char* sceneFilePath) {
	FILE *fp;
	char errbuf[200];

	fp = fopen(sceneFilePath, "r");
	if (!fp)
	{
		printf("Cannot open %s - %s\n", sceneFilePath, strerror(errno));
		return;
	}

	toml_table_t *level = toml_parse_file(fp, errbuf, sizeof(errbuf));
	fclose(fp);

	World *w = get_world(g);
	MemoryHeader *h = get_header(g);

	for (int i = 0;; i++)
	{
		const char *friendly_name = toml_key_in(level, i);
		if (!friendly_name)
			break;

		size_t entity = create_entity(w);
		set_entity_name(w,entity,friendly_name);
		
		toml_table_t *attributes = toml_table_in(level, friendly_name);
		if (attributes)
		{
			printf("[%s]\n", friendly_name);
			
			for (int j = 0;; j++)
			{
				const char *type_key = toml_key_in(attributes, j);
				if (!type_key)
					break;

				switch (mapStringToSubkeyType(type_key))
				{
				case POSITION_TYPE:
				case SCALE_TYPE:
					{
						toml_table_t *nested_table = toml_table_in(attributes, type_key);
						if (nested_table)
						{
							toml_datum_t x = toml_double_in(nested_table, "x");
							toml_datum_t y = toml_double_in(nested_table, "y");
							toml_datum_t z = toml_double_in(nested_table, "z");

							printf("  %s = { x = %.2f, y = %.2f, z = %.2f }\n", type_key, x.u.d, y.u.d, z.u.d);
						}
						break;
					}
				case ROTATION_TYPE:
					{
						toml_table_t *nested_table = toml_table_in(attributes, type_key);
						if (nested_table)
						{
							toml_datum_t x = toml_double_in(nested_table, "pitch");
							toml_datum_t y = toml_double_in(nested_table, "yaw");
							toml_datum_t z = toml_double_in(nested_table, "roll");

							printf("  %s = { x = %.2f, y = %.2f, z = %.2f }\n", type_key, x.u.d, y.u.d, z.u.d);
						}
						break;
					}
				case COLOR_TYPE:
					{
						toml_table_t *nested_table = toml_table_in(attributes, type_key);
						if (nested_table)
						{
							toml_datum_t r = toml_double_in(nested_table, "r");
							toml_datum_t g = toml_double_in(nested_table, "g");
							toml_datum_t b = toml_double_in(nested_table, "b");

							printf("  %s = { r = %.2f, g = %.2f, b = %.2f }\n", type_key, r.u.d, g.u.d, b.u.d);
						}
						break;
					}
				case CAMERA_TYPE:
					{
						toml_table_t *nested_table = toml_table_in(attributes, type_key);
						if (nested_table)
						{
							toml_datum_t fov_datum = toml_double_in(nested_table, "fov");
					        toml_datum_t near_datum = toml_double_in(nested_table, "near");
					        toml_datum_t far_datum = toml_double_in(nested_table, "far");

					        float fov = static_cast<float>(fov_datum.u.d);
					        float near = static_cast<float>(near_datum.u.d);
					        float far = static_cast<float>(far_datum.u.d);

							printf("  %s = { fov = %.2f, near = %.2f, far = %.2f }\n", type_key, fov, near, far);

							glm::vec3 cameraPosition = glm::vec3(0.0f,0.0f,0.0f);
							glm::vec3 targetPosition = glm::vec3(1.0f,0.0f,0.0f);
							glm::vec3 upDirection = glm::vec3(0.0f,0.0f,1.0f);

							glm::mat4 view = glm::lookAt(cameraPosition, targetPosition, upDirection);

							float aspectRatio = 16.0f/9.0f;

							glm::mat4 projection = glm::perspective(glm::radians(fov), aspectRatio, near, far);

							add_component(h, entity, COMPONENT_CAMERA);
						}
						break;
					}
				case MODEL_TYPE:
					{
						toml_datum_t model = toml_string_in(attributes, type_key);
						
						if (model.ok) {
		                    printf("  model = \"%s\"\n", model.u.s);
		                    if(LoadGLTFMeshes(h, model.u.s)) {
		                     printf("loading model to ecs\n");
		                    }
		                    free(model.u.s);
		                }
						break;
					}
				case MATERIAL_TYPE:
					{
						break;
					}
				case TEXTURE_TYPE:
					{
						break;
					}
				case FOV_TYPE:
					{
						toml_datum_t fov_value = toml_double_in(attributes, type_key);
						if (fov_value.ok)
							printf("  %s = %.2f\n", type_key, fov_value.u.d);
						break;
					}
				case INTENSITY_TYPE:
					{
						toml_datum_t intensity_value = toml_double_in(attributes, type_key);
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

void save_level(MemoryHeader* h, const char* saveFilePath) {
	FILE* fp = fopen(saveFilePath, "w");
	if (!fp) {
		printf("Failed to open file %s for writing.\n", saveFilePath);
		return;
	}


	World* w = &h->world;

	for (size_t i = 0; i < w->entity_count; ++i) {
		size_t entity_id = w->entity_ids[i];
		uint32_t mask = w->component_masks[entity_id];

        // Write the entity's name as the TOML table header
		fprintf(fp, "[%s]\n", w->entity_names[entity_id]);

        // Serialize each component type based on the component mask
		if (mask & COMPONENT_POSITION) {
			Vec3 position;
			if (get_component_value(h, entity_id, COMPONENT_POSITION, &position)) {
				fprintf(fp, "position = { x = %.2f, y = %.2f, z = %.2f }\n", position.x, position.y, position.z);
			}
		}

		// if (mask & COMPONENT_ROTATION) {
		// 	Vec3 rotation;
		// 	if (get_component_value(h, entity_id, COMPONENT_ROTATION, &rotation)) {
		// 		fprintf(fp, "rotation = { pitch = %.2f, yaw = %.2f, roll = %.2f }\n", rotation.x, rotation.y, rotation.z);
		// 	}
		// }

		// if (mask & COMPONENT_COLOR) {
		// 	Vec3 color;
		// 	if (get_component_value(h, entity_id, COMPONENT_COLOR, &color)) {
		// 		fprintf(fp, "color = { r = %.2f, g = %.2f, b = %.2f }\n", color.x, color.y, color.z);
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
		// 	if (get_component_value(h, entity_id, COMPONENT_INTENSITY, &intensity)) {
		// 		fprintf(fp, "intensity = %.2f\n", intensity);
		// 	}
		// }

        fprintf(fp, "\n"); // Add a newline between entities for readability
    }

    fclose(fp);
    printf("World saved to %s\n", saveFilePath);
}