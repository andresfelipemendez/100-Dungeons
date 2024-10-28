#pragma once

#include <SDL.h>
#include <entt/entt.hpp>


void ShootingSystem(entt::registry* registry, const Uint8* keyState);
void InputSystem(entt::registry* registry,const Uint8* state);
void MovementSystem(entt::registry* registry, float deltaTime);
void CollisionSystem(entt::registry* registry, float deltaTime);
void RenderSystem(struct GameData* gd, entt::registry* registry);