#pragma once
#include <toml.h>

#include <game.h>

#include <fstream>
#include <string>
#include <sstream>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <printLog.h>

void load_level(game* g, const char* levelFilePath) {
  FILE *fp; 
   
  char errbuf[200];

   fp = fopen("scene.toml", "r");
  if (!fp) {
    printf("cannot open sample.toml - ", strerror(errno));
  }

  toml_table_t *conf = toml_parse_file(fp, errbuf, sizeof(errbuf));
  fclose(fp);

  if (!conf) {
    printf("cannot parse - ", errbuf);
  }

  for (int i = 0; ; i++) {
        const char *key = toml_key_in(conf, i);
        if (!key) {
            break;
        }

        print_log("SUCCESS Found table: %s\n", COLOR_GREEN);

        toml_table_t *nested = toml_table_in(conf, key);
        if (nested) {
            printf("  %s is a table with nested values.\n", key);
            // Recursively list nested tables if needed
        }
    }
}



void write_scene_toml(const Scene *scene, const char *file_path) {
    FILE *file = fopen(file_path, "w");
    if (!file) {
        fprintf(stderr, "Failed to open file for writing: %s\n", file_path);
        return;
    }

    // Write camera data
    fprintf(file, "[camera]\n");
    fprintf(file, "position = { x = %.2f, y = %.2f, z = %.2f }\n", scene->camera.position.x, scene->camera.position.y, scene->camera.position.z);
    fprintf(file, "fov = %.2f\n\n", scene->camera.fov);

    // Write light data
    fprintf(file, "[light]\n");
    fprintf(file, "position = { x = %.2f, y = %.2f, z = %.2f }\n", scene->light.position.x, scene->light.position.y, scene->light.position.z);
    fprintf(file, "color = { r = %.2f, g = %.2f, b = %.2f }\n", scene->light.color.x, scene->light.color.y, scene->light.color.z);
    fprintf(file, "intensity = %.2f\n\n", scene->light.intensity);

    // Write mesh data
    fprintf(file, "[mesh]\n");
    fprintf(file, "position = { x = %.2f, y = %.2f, z = %.2f }\n", scene->mesh.position.x, scene->mesh.position.y, scene->mesh.position.z);
    fprintf(file, "scale = { x = %.2f, y = %.2f, z = %.2f }\n", scene->mesh.scale.x, scene->mesh.scale.y, scene->mesh.scale.z);
    fprintf(file, "rotation = { pitch = %.2f, yaw = %.2f, roll = %.2f }\n", scene->mesh.rotation.x, scene->mesh.rotation.y, scene->mesh.rotation.z);
    fprintf(file, "file = \"%s\"\n", scene->mesh.file);

    fclose(file);
}