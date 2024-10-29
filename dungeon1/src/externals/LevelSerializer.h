#pragma once
#include <toml.h>
#include <game.h>

void load_level(game* g, const char* levelFilePath);

void write_scene_toml(const Scene *scene, const char *file_path);