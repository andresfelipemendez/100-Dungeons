#include "systems.h"
#include "ecs.h"
#include "ext/matrix_transform.hpp"
#include "fwd.hpp"

#include <cstdio>
#include <glm.hpp>

#include <gtc/constants.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>

#include <glad.h>

#include <GLFW/glfw3.h>

#include "components.h"
#include "physics.h"
#include <game.h>

void input_system(game *g, Memory *m) {
  if (!get_entities(m, InputComponent | PositionComponent)) {
    return;
  }

  for (size_t i = 0; i < m->query.count; ++i) {
    size_t entity = m->query.entities[i];

    Position p;
    if (!get_component(m, entity, &p)) {
      continue;
    }

    float speed = 3.f;

    int joystickID = GLFW_JOYSTICK_1;
    if (glfwJoystickPresent(joystickID)) {
      int count;
      const float *axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &count);
      if (axes && count >= 2) {
        glm::vec2 v(axes[0], axes[1]);
        if (glm::length(v) > 0.5f) {
          v = glm::normalize(v);
          p.x += v.x * speed * g->deltaTime;
          p.z += v.y * speed * g->deltaTime;
          set_component(m, entity, p);
        }
      }
    }
  }
}

void camera_follow_system(game *g, Memory *m) {
  size_t camera_entity;
  if (!get_entity(m, CameraComponent, camera_entity)) {
    return;
  }

  size_t player_entity;
  if (!get_entity(m, InputComponent, player_entity)) {
    return;
  }

  Position camera_position;
  if (!get_component(m, camera_entity, &camera_position)) {
    return;
  }

  Position player_position;
  if (!get_component(m, player_entity, &player_position)) {
    return;
  }

  camera_position = {
      .x = player_position.x + 0,
      .y = player_position.y + 10,
      .z = player_position.z + 10,
  };
  set_component(m, camera_entity, camera_position);
}

glm::vec4 cc(0.45f, 0.55f, 0.60f, 1.00f);
void rendering_system(Memory *m) {
  glClearColor(cc.x * cc.a, cc.y * cc.a, cc.z * cc.a, cc.a);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);

  size_t camera_entity;
  if (!get_entity(m, CameraComponent, camera_entity)) {
    printf("couldn't find camera entity\n");
  }

  Position p;
  if (!get_component(m, camera_entity, &p)) {
    return;
  }

  Rotation r;
  if (!get_component(m, camera_entity, &r)) {
    return;
  }

  glm::vec3 cameraPosition = glm::vec3(p.x, p.y, p.z);
  glm::vec3 targetPosition = glm::vec3(1.0f, 0.0f, 0.0f);
  glm::vec3 upDirection = glm::vec3(0.0f, 1.0f, 0.0f);

  glm::quat orientation = glm::quat(r.w, r.x, r.y, r.z);
  glm::mat4 rotMat = glm::mat4_cast(orientation);
  glm::mat4 traslationMat = glm::translate(glm::mat4(1.0f), -cameraPosition);

  glm::mat4 view = rotMat * traslationMat;

  float aspectRatio = 16.0f / 9.0f;

  Camera c;
  if (!get_component(m, camera_entity, &c)) {
    printf("couldn't find camera entity\n");
  }
  glm::mat4 projection =
      glm::perspective(glm::radians(c.fov), aspectRatio, c.near, c.far);
  glm::mat4 viewProj = projection * view;

  if (!get_entities(m,
                    MaterialComponent | ModelComponent | PositionComponent)) {
    return;
  }

  for (size_t i = 0; i < m->query.count; ++i) {
    Material material;
    if (get_component(m, m->query.entities[i], &material)) {
      glUseProgram(material.shader_id);
    }
    glm::mat4 worldTransform = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));

    Position p;
    if (get_component(m, m->query.entities[i], &p)) {
      GLuint view_loc = glGetUniformLocation(material.shader_id, "uViewProj");
      glUniformMatrix4fv(view_loc, 1, GL_TRUE, &viewProj[0][0]);

      worldTransform = glm::translate(worldTransform, glm::vec3{p.x, p.y, p.z});
      GLuint loc = glGetUniformLocation(material.shader_id, "uWorldTransform");
      glUniformMatrix4fv(loc, 1, GL_TRUE, &worldTransform[0][0]);
    }

    Model model;
    if (get_component(m, m->query.entities[i], &model)) {
      glBindBuffer(GL_DRAW_INDIRECT_BUFFER, model.drawsBuffer);
      for (auto i = 0U; i < model.submesh_count; ++i) {
        auto &submesh = model.submeshes[i];
        glBindVertexArray(submesh.vertexArray);
        glDrawElementsIndirect(
            GL_TRIANGLES, submesh.indexType,
            reinterpret_cast<const void *>(i * sizeof(SubMesh)));
      }
    }
  }
  GLenum err;
  while ((err = glGetError()) != GL_NO_ERROR) {
    printf("OpenGL error: %d\n", err);
  }

  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  if (get_entities(m, ColliderComponent | PositionComponent)) {
  }
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void systems(game *g, Memory *m) {
  input_system(g, m);
  physics_system(g, m);
  camera_follow_system(g, m);
  rendering_system(m);
}
