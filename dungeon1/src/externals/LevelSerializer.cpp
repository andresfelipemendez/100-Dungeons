#include "LevelSerializer.h"

#include <fstream>
#include <string>
#include <sstream>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <printLog.h>

#define SUBKEY_TYPES \
    X(position)      \
    X(color)         \
    X(scale)         \
    X(rotation)      \
    X(fov)           \
    X(intensity)     \
    X(file)

enum SubkeyType {
    #define X(name) name##_TYPE,
    SUBKEY_TYPES
    #undef X
    UNKNOWN_TYPE
};

SubkeyType mapStringToSubkeyType(const char* type_key) {
    #define X(name) if (strcmp(type_key, #name) == 0) return name##_TYPE;
    SUBKEY_TYPES
    #undef X
    return UNKNOWN_TYPE;
}

void load_level(game* g, const char* levelFilePath) {
    FILE *fp;
    char errbuf[200];

    fp = fopen(levelFilePath, "r");
    if (!fp) {
        printf("Cannot open %s - %s\n", levelFilePath, strerror(errno));
        return;
    }

    toml_table_t *level = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (!level) {
        print_log("ERROR cannot parse - %s\n", errbuf);
        return;
    }

    for (int i = 0; ; i++) {
        const char *friendly_name = toml_key_in(level, i);
        if (!friendly_name) break;

        toml_table_t *attributes = toml_table_in(level, friendly_name);
        if (attributes) {
            printf("[%s]\n", friendly_name);

            for (int j = 0; ; j++) {
                const char *type_key = toml_key_in(attributes, j);
                if (!type_key) break;

                 switch (mapStringToSubkeyType(type_key)) {
                    case position_TYPE: case scale_TYPE:  {
                        toml_table_t *nested_table = toml_table_in(attributes, type_key);
                        if (nested_table) {
                            toml_datum_t x = toml_double_in(nested_table, "x");
                            toml_datum_t y = toml_double_in(nested_table, "y");
                            toml_datum_t z = toml_double_in(nested_table, "z");

                            printf("  %s = { x = %.2f, y = %.2f, z = %.2f }\n", type_key, x.u.d, y.u.d, z.u.d);
                        }
                        break;
                    }
                    case rotation_TYPE:{
                        toml_table_t *nested_table = toml_table_in(attributes, type_key);
                        if (nested_table) {
                            toml_datum_t x = toml_double_in(nested_table, "pitch");
                            toml_datum_t y = toml_double_in(nested_table, "yaw");
                            toml_datum_t z = toml_double_in(nested_table, "roll");

                            printf("  %s = { x = %.2f, y = %.2f, z = %.2f }\n", type_key, x.u.d, y.u.d, z.u.d);
                        }
                        break;
                    }
                    case color_TYPE: {
                        toml_table_t *nested_table = toml_table_in(attributes, type_key);
                        if (nested_table) {
                            toml_datum_t r = toml_double_in(nested_table, "r");
                            toml_datum_t g = toml_double_in(nested_table, "g");
                            toml_datum_t b = toml_double_in(nested_table, "b");

                            printf("  %s = { r = %.2f, g = %.2f, b = %.2f }\n", type_key, r.u.d, g.u.d, b.u.d);
                        }
                        break;
                    }
                    case fov_TYPE: {
                        toml_datum_t fov_value = toml_double_in(attributes, type_key);
                        if (fov_value.ok) printf("  %s = %.2f\n", type_key, fov_value.u.d);
                        break;
                    }
                    case intensity_TYPE: {
                        toml_datum_t intensity_value = toml_double_in(attributes, type_key);
                        if (intensity_value.ok) printf("  %s = %.2f\n", type_key, intensity_value.u.d);
                        break;
                    }
                    case file_TYPE: {
                        toml_datum_t file_value = toml_string_in(attributes, type_key);
                        if (file_value.ok) printf("  %s = \"%s\"\n", type_key, file_value.u.s);
                        free(file_value.u.s);
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



void write_scene_toml(const World *scene, const char *file_path) {
    // FILE *file = fopen(file_path, "w");
    // if (!file) {
    //     fprintf(stderr, "Failed to open file for writing: %s\n", file_path);
    //     return;
    // }

    // // Write camera data
    // fprintf(file, "[camera]\n");
    // fprintf(file, "position = { x = %.2f, y = %.2f, z = %.2f }\n", scene->camera.position.x, scene->camera.position.y, scene->camera.position.z);
    // fprintf(file, "fov = %.2f\n\n", scene->camera.fov);

    // // Write light data
    // fprintf(file, "[light]\n");
    // fprintf(file, "position = { x = %.2f, y = %.2f, z = %.2f }\n", scene->light.position.x, scene->light.position.y, scene->light.position.z);
    // fprintf(file, "color = { r = %.2f, g = %.2f, b = %.2f }\n", scene->light.color.x, scene->light.color.y, scene->light.color.z);
    // fprintf(file, "intensity = %.2f\n\n", scene->light.intensity);

    // // Write mesh data
    // fprintf(file, "[mesh]\n");
    // fprintf(file, "position = { x = %.2f, y = %.2f, z = %.2f }\n", scene->mesh.position.x, scene->mesh.position.y, scene->mesh.position.z);
    // fprintf(file, "scale = { x = %.2f, y = %.2f, z = %.2f }\n", scene->mesh.scale.x, scene->mesh.scale.y, scene->mesh.scale.z);
    // fprintf(file, "rotation = { pitch = %.2f, yaw = %.2f, roll = %.2f }\n", scene->mesh.rotation.x, scene->mesh.rotation.y, scene->mesh.rotation.z);
    // fprintf(file, "file = \"%s\"\n", scene->mesh.file);

    // fclose(file);
}