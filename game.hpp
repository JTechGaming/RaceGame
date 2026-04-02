#pragma once

#include "IGame.hpp"
#include "transform.hpp"
#include "camera.hpp"
#include "physicsEngine.hpp"

#include "mathUtils.hpp"

struct CarInput {
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
    bool brake = false;
};

class RaceGame : public IGame {
public:
    void OnInit(AssetManager* assetManager, SceneManager* sceneManager, std::string basePath) override {
        m_assetManager = assetManager;
        m_sceneManager = sceneManager;

        objects.reserve(3);

        Object car{};
        car.model = assetManager->loadModel(AssetManager::buildModelPath("models/car2"));
        car.modelTransform = Transform{glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f), glm::vec3(1.0f)};
        objects.insert({"Player", car});

        RigidBody* playerBody = new RigidBody(glm::vec3(0, 2, 0), glm::vec3(1.0f, 0.5f, 2.0f), 1200.0f);
        physicsEngine.addBody(playerBody);
        bodyMap["Player"] = playerBody;

        camera.attachToObject(&objects["Player"], glm::vec3(0.0f, 2.0f, 0.0f), 5.0f, -15.0f, 0.0f, {45.0f, 360.0f}, {2.0f, 10.0f});

        std::vector<std::string> others = {"Fake1", "Fake2"};
        std::vector<glm::vec3> positions = {glm::vec3(3, 2, 0), glm::vec3(-3, 2, 0)};

        for (int i = 0; i < (int)others.size(); i++) {
            Object otherCar{};
            otherCar.model = assetManager->loadModel(AssetManager::buildModelPath("models/car2"));
            otherCar.modelTransform = Transform{positions[i], glm::vec3(0.0f), glm::vec3(1.0f)};
            objects.insert({others[i], otherCar});

            RigidBody* rb = new RigidBody(positions[i], glm::vec3(1.0f, 0.5f, 2.0f), 1200.0f);
            physicsEngine.addBody(rb);
            bodyMap[others[i]] = rb;
        }

        // Thin slab floor top surface at y=0
        AABB* floorCollider = new AABB(glm::vec3(0.0f, -0.05f, 0.0f), glm::vec3(100.0f, 0.05f, 100.0f));
        physicsEngine.addStaticCollider(floorCollider);

        debugCube = m_assetManager->loadModel(AssetManager::buildModelPath("models/cube"));
    }

    void OnShutdown() override {}

    void OnTick(float deltaTime) override {
        std::vector<std::string> carIds = {"Player", "Fake1", "Fake2"};

        for (const std::string& id : carIds) {
            RigidBody* rb = bodyMap[id];
            CarInput input;

            if (id == "Player") {
                input.forward  = isKeyPressed(GLFW_KEY_W);
                input.backward = isKeyPressed(GLFW_KEY_S);
                input.left     = isKeyPressed(GLFW_KEY_A);
                input.right    = isKeyPressed(GLFW_KEY_D);
                input.brake    = isKeyPressed(GLFW_KEY_SPACE);
            } else {
                // AI logic
                // IGOORRRRR

                glm::vec3 bot1DestinationPoint = m_sceneManager->getScene().trackSpline._points[bot1NextSplineIndex];
            }

            // Physics
            glm::mat3 rotationMat = glm::mat3_cast(rb->m_orientation);
            glm::vec3 forward = rotationMat[2];
            glm::vec3 right   = rotationMat[0];

            const float engineForce = 15000.0f;
            const float turnTorque  = 20000.0f;
            const float maxSpeed    = 50.0f;
            float forwardSpeed = glm::dot(rb->m_velocity, forward);

            if (rb->m_onGround) {
                // Acceleration
                if (input.forward && forwardSpeed < maxSpeed)
                    rb->m_velocity += forward * engineForce * rb->m_inverseMass * deltaTime;
                if (input.backward && forwardSpeed > -maxSpeed * 0.5f)
                    rb->m_velocity -= forward * engineForce * rb->m_inverseMass * deltaTime;

                // Steering
                float grip = glm::clamp(std::abs(forwardSpeed) / 5.0f, 0.0f, 1.0f);
                float turnDir = (forwardSpeed >= 0.0f) ? 1.0f : -1.0f;

                if (input.left)
                    rb->m_angularVelocity.y += turnTorque * rb->m_inertiaTensorInv[1][1] * grip * turnDir * deltaTime;
                if (input.right)
                    rb->m_angularVelocity.y -= turnTorque * rb->m_inertiaTensorInv[1][1] * grip * turnDir * deltaTime;

                // Braking and drifting
                float driftFactor = input.brake ? 0.2f : 1.0f;
                if (input.brake) {
                    rb->m_velocity *= (1.0f - 0.99f * deltaTime);
                }

                // Lateral grip (Sideways sliding)
                float lateralSpeed = glm::dot(rb->m_velocity, right);
                rb->m_velocity -= right * lateralSpeed * (0.85f * driftFactor);

                // Rolling resistance
                if (!input.forward && !input.backward) {
                    rb->m_velocity -= forward * forwardSpeed * 0.005f;
                }
            }

            // damping (air resistance)
            rb->m_velocity        *= 0.999f;
            rb->m_angularVelocity *= 0.90f;
        }

        physicsEngine.step(deltaTime);

        // Sync to renderer
        for (auto const& [id, body] : bodyMap) {
            objects[id].modelTransform.pos = body->m_position;
            objects[id].modelTransform.rot = glm::eulerAngles(body->m_orientation);
        }
    }

    void OnPreRender(Shader& shader, float deltaTime) override {}

    void OnPostRender(Shader& shader, float deltaTime) override {
        for (auto& el : objects)
            el.second.model->draw(shader, el.second.modelTransform);

        if(m_debug) {
            for (glm::vec3 point : m_sceneManager->getScene().trackSpline._points) {
                shader.setVec3("material.baseColor", glm::vec3(1.0f, 0.0f, 0.0f)); // red for debug
                Transform debugTransform{point, glm::vec3(0.0f), glm::vec3(0.1f)};
                debugCube->draw(shader, debugTransform);
            }
        }
    }

    void setDebug(bool debug) override { m_debug = debug; }
    bool getDebug() const override { return m_debug; }

    Camera& getCamera() { return camera; }

private:
    AssetManager* m_assetManager;
    SceneManager* m_sceneManager;

    std::unordered_map<std::string, Object> objects;
    std::unordered_map<std::string, RigidBody*> bodyMap;
    SimplePhysicsEngine physicsEngine;
    Camera camera;
    bool m_debug = false;

    ModelResource* debugCube = nullptr;

    uint16_t bot1NextSplineIndex = 0;
    uint16_t bot2NextSplineIndex = 0;

    std::unordered_map<std::string, RacingVehicle> vehicles;
};
