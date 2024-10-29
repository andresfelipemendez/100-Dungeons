#pragma once
typedef void *(*ImGuiMemAllocFunc)(size_t sz, void *user_data);
typedef void (*ImGuiMemFreeFunc)(void *ptr, void *user_data);

struct game {
  int play;
  struct GLFWwindow *window;
  struct ImGuiContext *ctx;
  ImGuiMemAllocFunc alloc_func;
  ImGuiMemFreeFunc free_func;
  void *user_data;
  void *engine_lib;
  void *world;
};

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    Vec3 position;
    float fov;
} Camera;

typedef struct {
    Vec3 position;
    Vec3 color;
    float intensity;
} Light;

typedef struct {
    Vec3 position;
    Vec3 scale;
    Vec3 rotation;
    const char *file;
} Mesh;

typedef struct {
    Camera camera;
    Light light;
    Mesh mesh;
} Scene;