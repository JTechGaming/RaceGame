#pragma once

#include "IGame.hpp"
#include "transform.hpp"
#include "camera.hpp"
#include "physicsEngine.hpp"

class RaceGame : public IGame {
public:
    void OnInit(AssetManager* assetManager, SceneManager* sceneManager, std::string basePath) override {
        objects.reserve(3);

        // Create the player
        Object car{};
        car.model = assetManager->loadModel(AssetManager::buildModelPath("models/car2"));
        car.modelTransform = Transform{glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(0.0f), glm::vec3(1.0f)};
        objects.insert({"Player", car});

        // Create RigidBody for Player (pos, halfExtents, mass)
        RigidBody* playerBody = new RigidBody(glm::vec3(0, 5, 0), glm::vec3(1.0f, 0.5f, 2.0f), 1200.0f);
        physicsEngine.addBody(playerBody);
        bodyMap["Player"] = playerBody;

        camera.attachToObject(&objects["Player"], glm::vec3(0.0f, 2.0f, 0.0f), 5.0f, -15.0f, 0.0f, {45.0f, 360.0f}, {2.0f, 10.0f});

        // Create the AI cars
        std::vector<std::string> others = {"Fake1", "Fake2"};
        std::vector<glm::vec3> positions = {glm::vec3(3, 5, 0), glm::vec3(-3, 5, 0)};

        for(int i=0; i < others.size(); i++) {
            Object otherCar{};
            otherCar.model = assetManager->loadModel(AssetManager::buildModelPath("models/car2"));
            otherCar.modelTransform = Transform{positions[i], glm::vec3(0.0f), glm::vec3(1.0f)};
            objects.insert({others[i], otherCar});

            RigidBody* rb = new RigidBody(positions[i], glm::vec3(1.0f, 0.5f, 2.0f), 1200.0f);
            physicsEngine.addBody(rb);
            bodyMap[others[i]] = rb;
        }

        AABB* floorCollider = new AABB(glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(100.0f, 1.0f, 100.0f));
        physicsEngine.addStaticCollider(floorCollider);
    }

    void OnShutdown() override {

    }

    void OnTick(float deltaTime) override {
        RigidBody* player = bodyMap["Player"];

        // Determine direction based on the current rotation of the RigidBody
        glm::mat3 rotationMat = glm::mat3_cast(player->m_orientation);
        glm::vec3 forward = rotationMat[2]; // Z-axis of the OBB
        glm::vec3 right = rotationMat[0];   // X-axis of the OBB

        // input to force
        float engineForce = 15000.0f;
        float turnTorque = 8000.0f;

        if (isKeyPressed(GLFW_KEY_W)) {
            player->m_velocity += forward * engineForce * player->m_inverseMass * deltaTime;
        }
        if (isKeyPressed(GLFW_KEY_S)) {
            player->m_velocity -= forward * engineForce * player->m_inverseMass * deltaTime;
        }
        if (isKeyPressed(GLFW_KEY_A)) {
            player->m_angularVelocity.y += turnTorque * player->m_inertiaTensorInv[1][1] * deltaTime;
        }
        if (isKeyPressed(GLFW_KEY_D)) {
            player->m_angularVelocity.y -= turnTorque * player->m_inertiaTensorInv[1][1] * deltaTime;
        }

        // Simple air resistance (Drag) to prevent infinite acceleration
        player->m_velocity *= 0.98f;
        player->m_angularVelocity *= 0.95f;

        physicsEngine.step(deltaTime);

        // physics to world objects
        for (auto const& [id, body] : bodyMap) {
            objects[id].modelTransform.pos = body->m_position;
            // Convert Quaternion back to Euler for the Transform class
            objects[id].modelTransform.rot = glm::eulerAngles(body->m_orientation);
        }
    }

    void OnPreRender(Shader& shader, float deltaTime) override {}

    void OnPostRender(Shader& shader, float deltaTime) override {
        for (auto& el : objects) {
            el.second.model->draw(shader, el.second.modelTransform);
        }
    }

    void setDebug(bool debug) override {}
    bool getDebug() const override {}

    Camera& getCamera() { return camera; }

private:
    std::unordered_map<std::string, Object> objects;
    std::unordered_map<std::string, RigidBody*> bodyMap;
    SimplePhysicsEngine physicsEngine;
    Camera camera;

    std::unordered_map<std::string, RacingVehicle> vehicles;
};