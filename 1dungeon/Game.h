#pragma once
#include "SDL_video.h"
#include "entt/entity/fwd.hpp"
#include <SDL.h>
#include <entt/entt.hpp>
#include "Components.h"

struct GameData {
  SDL_Window *window = nullptr;
  Shader* shader;
  VertexArray* vertexArray;
  SDL_GLContext context;
  Uint32 ticksCount = 0;
  bool isRunning = false;
};

bool Initialize(GameData *gd, entt::registry *registry);
void RunLoop(GameData *gd, entt::registry *registry);
void ShutDown(GameData *gd);

void ProcessInput(GameData *gd, entt::registry *registry);
void UpdateGame(GameData *gd, entt::registry *registry);
void GenerateOutput(GameData *gd, entt::registry *registry);

void LoadData(GameData *gd, entt::registry *registry);
void UnloadData(GameData *gd);

Shader GetShader(const std::string& vertexShader, const std::string& fragmentShader);
Texture GetTexture(const std::string &fileName);
unsigned int GetVertexArray();
