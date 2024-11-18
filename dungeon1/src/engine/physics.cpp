#include "physics.h"
#include "ecs.h"
#include "fwd.hpp"
#include <ext/matrix_transform.hpp>
#include <game.h>
#include <glm.hpp>
#include <gtc/constants.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>

void physics_system(game *g, MemoryHeader *h) {

    if (!get_entities(h, RigidBodyComponent | PositionComponent | ForceAccumulatorComponent | VelocityComponent)) {
        return;
    }

    for (size_t i = 0; i < h->query.count; ++i) {
        size_t entity = h->query.entities[i];

        RigidBody rb;
        get_component(h, entity, &rb);
        if (rb.invMass <= 0.0f) {
            continue;
        }

        ForceAccumulator accumulator;
        get_component(h, entity, &accumulator);

        Velocity velocity;
        get_component(h, entity, &velocity);

        Position pos;
        get_component(h, entity, &pos);

        // Step 1: Integrate forces to update velocity
        glm::vec3 acceleration = accumulator.force * rb.invMass;
        velocity.linear += acceleration * (float)g->deltaTime;

        // Apply damping to linear velocity
        velocity.linear *= pow(rb.linearDamping, (float)g->deltaTime);

        // Integrate angular forces (torques) to update angular velocity
        glm::vec3 angularAcceleration = accumulator.torque * rb.invMass;
        velocity.angular += angularAcceleration * (float)g->deltaTime;

        // Apply damping to angular velocity
        velocity.angular *= pow(rb.angularDamping, g->deltaTime);

        // Step 2: Integrate velocity to update position
        pos.x += velocity.linear.x * g->deltaTime;
        pos.y += velocity.linear.y * g->deltaTime;
        pos.z += velocity.linear.z * g->deltaTime;
        set_component(h, entity, pos);

        // Step 3: Update rotation based on angular velocity
        Rotation rot;
        get_component(h, entity, &rot);
        glm::quat orientation = glm::quat(rot.w, rot.x, rot.y, rot.z);
        glm::vec3 angularVelocity = velocity.angular * (float)g->deltaTime;
        glm::quat deltaRotation = glm::quat(0, angularVelocity.x, angularVelocity.y, angularVelocity.z) * orientation * 0.5f;
        orientation += deltaRotation;
        orientation = glm::normalize(orientation);
        rot.w = orientation.w;
        rot.x = orientation.x;
        rot.y = orientation.y;
        rot.z = orientation.z;
        set_component(h, entity, rot);

        // Clear force accumulators for the next frame
        accumulator.force = glm::vec3(0.0f);
        accumulator.torque = glm::vec3(0.0f);

        // Update components after modifications
        set_component(h, entity, accumulator);
        set_component(h, entity, velocity);
        set_component(h, entity, rb);
    }
}
