#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <algorithm>

struct AABB {
    glm::vec3 center;
    glm::vec3 halfExtents;
    AABB(glm::vec3 c = glm::vec3(0), glm::vec3 he = glm::vec3(1)) : center(c), halfExtents(he) {}
    glm::vec3 getMin() const { return center - halfExtents; }
    glm::vec3 getMax() const { return center + halfExtents; }
};

struct OBB {
    glm::vec3 center;
    glm::vec3 halfExtents;
    glm::mat3 rotation; 

    std::vector<glm::vec3> getVertices() const {
        std::vector<glm::vec3> v(8);
        glm::vec3 r = rotation[0] * halfExtents.x;
        glm::vec3 u = rotation[1] * halfExtents.y;
        glm::vec3 f = rotation[2] * halfExtents.z;

        v[0] = center - r - u - f; v[1] = center + r - u - f;
        v[2] = center - r + u - f; v[3] = center + r + u - f;
        v[4] = center - r - u + f; v[5] = center + r - u + f;
        v[6] = center - r + u + f; v[7] = center + r + u + f;
        return v;
    }
};

struct Wheel {
    glm::vec3 connectionPoint;
    float suspensionRestLength = 0.5f;
    float springStiffness = 5000.0f; // Increased for heavy cars
    float damperStiffness = 500.0f;
    float wheelRadius = 0.3f;
    float friction = 0.8f;
    float lastCompression = 0.0f;
};

struct RigidBody {
    glm::vec3 m_position;
    glm::quat m_orientation;
    glm::vec3 m_velocity;
    glm::vec3 m_angularVelocity;
    
    float m_mass;
    float m_inverseMass;
    glm::mat3 m_inertiaTensorInv;
    OBB m_obb;

    RigidBody(glm::vec3 pos, glm::vec3 extents, float mass) 
        : m_position(pos), m_orientation(glm::vec3(0)), m_velocity(0), 
          m_angularVelocity(0), m_mass(mass) {
        
        m_inverseMass = (mass > 0) ? 1.0f / mass : 0.0f;
        m_obb.center = pos;
        m_obb.halfExtents = extents;
        m_obb.rotation = glm::mat3_cast(m_orientation);

        // Basic moments of inertia for a box
        float x2 = (extents.x * 2) * (extents.x * 2);
        float y2 = (extents.y * 2) * (extents.y * 2);
        float z2 = (extents.z * 2) * (extents.z * 2);
        float ix = (1.0f / 12.0f) * mass * (y2 + z2);
        float iy = (1.0f / 12.0f) * mass * (x2 + z2);
        float iz = (1.0f / 12.0f) * mass * (x2 + y2);
        m_inertiaTensorInv = glm::inverse(glm::mat3(ix, 0, 0, 0, iy, 0, 0, 0, iz));
    }

    void updateTransform() {
        m_obb.center = m_position;
        m_obb.rotation = glm::mat3_cast(m_orientation);
    }
};

struct CollisionInfo {
    bool colliding;
    glm::vec3 normal;
    float penetration;
};

class RacingVehicle {
public:
    RigidBody* body;
    std::vector<Wheel> wheels;

    void updateSuspension(float dt, const std::vector<AABB*>& world) {
        for (auto& wheel : wheels) {
            glm::vec3 worldWheelPos = body->m_position + (body->m_obb.rotation * wheel.connectionPoint);
            glm::vec3 rayDir = body->m_obb.rotation[1] * -1.0f; 

            float distance = castRay(worldWheelPos, rayDir, wheel.suspensionRestLength + wheel.wheelRadius, world);

            if (distance < wheel.suspensionRestLength + wheel.wheelRadius) {
                float compression = (wheel.suspensionRestLength + wheel.wheelRadius) - distance;
                float springForce = compression * wheel.springStiffness;
                float suspensionVelocity = (compression - wheel.lastCompression) / dt;
                float damperForce = suspensionVelocity * wheel.damperStiffness;
                wheel.lastCompression = compression;

                glm::vec3 totalUpForce = rayDir * -(springForce + damperForce);
                applyForceAtPoint(totalUpForce, worldWheelPos);

                // Grip
                glm::vec3 wheelVelocity = body->m_velocity + glm::cross(body->m_angularVelocity, worldWheelPos - body->m_position);
                glm::vec3 sideDir = body->m_obb.rotation[0]; 
                float sideVel = glm::dot(wheelVelocity, sideDir);
                glm::vec3 frictionImpulse = -sideDir * (sideVel * wheel.friction * body->m_mass * dt); // Force instead of impuls
                applyForceAtPoint(frictionImpulse, worldWheelPos);
            }
        }
    }

    void applyForceAtPoint(glm::vec3 force, glm::vec3 worldPoint) {
        body->m_velocity += (force * body->m_inverseMass);
        glm::vec3 r = worldPoint - body->m_position;
        body->m_angularVelocity += body->m_inertiaTensorInv * glm::cross(r, force);
    }

    float castRay(glm::vec3 origin, glm::vec3 dir, float maxDist, const std::vector<AABB*>& world) {
        if (origin.y < maxDist) return origin.y; 
        return maxDist + 1.0f;
    }
};

// Collision Detection (SAT)
inline void projectOBB(const OBB& box, const glm::vec3& axis, float& min, float& max) {
    auto vertices = box.getVertices();
    min = max = glm::dot(vertices[0], axis);
    for (int i = 1; i < 8; i++) {
        float p = glm::dot(vertices[i], axis);
        min = std::min(min, p);
        max = std::max(max, p);
    }
}

inline CollisionInfo checkOBBCollision(const OBB& a, const OBB& b) {
    CollisionInfo result{false, glm::vec3(0), FLT_MAX};
    glm::vec3 axes[6] = { a.rotation[0], a.rotation[1], a.rotation[2], b.rotation[0], b.rotation[1], b.rotation[2] };

    for (int i=0; i<6; i++) {
        glm::vec3 axis = glm::normalize(axes[i]);
        float minA, maxA, minB, maxB;
        projectOBB(a, axis, minA, maxA);
        projectOBB(b, axis, minB, maxB);
        float overlap = std::max(0.0f, std::min(maxA, maxB) - std::max(minA, minB));
        if (overlap <= 0.0f) return {false, glm::vec3(0), 0.0f};
        if (overlap < result.penetration) { result.penetration = overlap; result.normal = axis; }
    }
    if (glm::dot(b.center - a.center, result.normal) < 0) result.normal = -result.normal;
    result.colliding = true;
    return result;
}

inline CollisionInfo checkOBBvsAABB(const OBB& a, const AABB& b) {
    OBB bAsObb;
    bAsObb.center = b.center;
    bAsObb.halfExtents = b.halfExtents;
    bAsObb.rotation = glm::mat3(1.0f); // Identity matrix = no rotation
    return checkOBBCollision(a, bAsObb);
}

// Physics Engine
class SimplePhysicsEngine {
public:
    std::vector<RigidBody*> bodies;
    std::vector<AABB*> staticColliders;
    glm::vec3 gravity{0, -9.81f, 0};
    const int SUB_STEPS = 8;

    void addBody(RigidBody* body) { bodies.push_back(body); }
    void addStaticCollider(AABB* collider) { staticColliders.push_back(collider); }

    void step(float dt) {
        float subDt = dt / (float)SUB_STEPS;
        for (int s = 0; s < SUB_STEPS; s++) {
            for (auto b : bodies) {
                for (auto staticCollider : staticColliders) {
                    auto col = checkOBBvsAABB(b->m_obb, *staticCollider);
                    if (col.colliding) {
                        // push car up and stop vertical velocity
                        b->m_position += col.normal * col.penetration;
                        b->updateTransform();

                        float velDot = glm::dot(b->m_velocity, col.normal);
                        if (velDot < 0) {
                            b->m_velocity -= col.normal * velDot;
                        }
                    }
                }
                if (b->m_mass <= 0) continue;
                b->m_velocity += gravity * subDt;
                b->m_position += b->m_velocity * subDt;
                glm::quat q = glm::quat(0, b->m_angularVelocity * subDt);
                b->m_orientation += (q * 0.5f) * b->m_orientation;
                b->m_orientation = glm::normalize(b->m_orientation);
                b->updateTransform();
            }
            for (size_t i = 0; i < bodies.size(); i++) {
                for (size_t j = i + 1; j < bodies.size(); j++) {
                    auto col = checkOBBCollision(bodies[i]->m_obb, bodies[j]->m_obb);
                    if (col.colliding) resolveCollision(bodies[i], bodies[j], col);
                }
            }
        }
    }

private:
    void resolveCollision(RigidBody* a, RigidBody* b, CollisionInfo col) {
        float totalInvMass = a->m_inverseMass + b->m_inverseMass;
        if (totalInvMass == 0) return;
        glm::vec3 rv = b->m_velocity - a->m_velocity;
        float velAlongNormal = glm::dot(rv, col.normal);
        if (velAlongNormal > 0) return;
        float e = 0.2f;
        float j = -(1.0f + e) * velAlongNormal / totalInvMass;
        glm::vec3 impulse = j * col.normal;
        a->m_velocity -= impulse * a->m_inverseMass;
        b->m_velocity += impulse * b->m_inverseMass;
        // Positionele correctie
        glm::vec3 correction = (std::max(col.penetration - 0.01f, 0.0f) / totalInvMass) * 0.2f * col.normal;
        a->m_position -= correction * a->m_inverseMass;
        b->m_position += correction * b->m_inverseMass;
    }
};