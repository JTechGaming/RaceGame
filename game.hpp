#pragma once

#include "IGame.hpp"

#include "transform.hpp"

#include "camera.hpp"

#include <iostream>

#include "dynamicObject.hpp"

//(voor Igor) attachToObject

class RaceGame : public IGame {
public:
    /*
        Can: Create objects, assign transforms to objects, run calculations on objects
        Cannot: Render objects, access render data of objects, assign object ubo's
    */
    void OnInit(AssetManager* assetManager, SceneManager* sceneManager, std::string basePath) override {
        objects.reserve(3);

        Object car{};
        car.model = assetManager->loadModel(AssetManager::buildModelPath("models/car2"));
        car.modelTransform = Transform{glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f)};
        objects.insert({"Player", car});

        camera.attachToObject(&objects["Player"], glm::vec3(0.0f, 2.0f, 0.0f), 5.0f, -15.0f, 0.0f, {45.0f, 360.0f}, {2.0f, 10.0f});

        Object car2{};
        car2.model = assetManager->loadModel(AssetManager::buildModelPath("models/car2"));
        car2.modelTransform = Transform{glm::vec3(3.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f)};
        objects.insert({"Fake1", car2});

        Object car3{};
        car3.model = assetManager->loadModel(AssetManager::buildModelPath("models/car2"));
        car3.modelTransform = Transform{glm::vec3(-3.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f)};
        objects.insert({"Fake2", car3});
    }

    /*
        Cannot: Render objects, access render data of objects, assign object ubo's, create objects, assign transforms to objects, run calculations on objects
    */
    void OnShutdown() override {
        
    }

    /*
        Can: Create objects, assign transforms to objects, run calculations on objects
        Cannot: Render objects, access render data of objects, assign object ubo's
    */
    void OnTick(float deltaTime) override {
        // Player movement, calculate forward direction based on yaw rotation
        Object& player = objects.at("Player");
        float yaw = player.modelTransform.rot.y;
        glm::vec3 moveDir = glm::vec3(sin(yaw), 0.0f, cos(yaw)); // Forward direction based on rotation

        if (isKeyPressed(GLFW_KEY_W)) {
            player.modelTransform.pos = moveForward(player.modelTransform.pos, moveDir, carSpeed, deltaTime);
        }
        if (isKeyPressed(GLFW_KEY_S)) {
            player.modelTransform.pos = moveBackward(player.modelTransform.pos, moveDir, carSpeed, deltaTime);
        }
        if (isKeyPressed(GLFW_KEY_A)) {
            player.modelTransform.rot.y += rotationSpeed * deltaTime;
        }
        if (isKeyPressed(GLFW_KEY_D)) {
            player.modelTransform.rot.y -= rotationSpeed * deltaTime;
        }
        if (isKeyPressed(GLFW_KEY_SPACE)) {
            player.modelTransform.pos = moveUp(player.modelTransform.pos, glm::vec3(0.0f, 1.0f, 0.0f), carSpeed, deltaTime);
        }
        if (isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
            player.modelTransform.pos = moveDown(player.modelTransform.pos, glm::vec3(0.0f, 1.0f, 0.0f), carSpeed, deltaTime);
        }

        //
        //  Dynamic Object Movement
        //
        objects.at("Fake1").modelTransform.pos += glm::vec3(0.0f, 0.0f, 6.0f * deltaTime);
        objects.at("Fake2").modelTransform.pos += glm::vec3(0.0f, 0.0f, 1.0f * deltaTime);
    }

    /*
        Can: Render objects, access render data of objects, assign object ubo's, assign transforms to objects (before drawing them), run calculations on objects
        Cannot: Create objects
    */
    void OnPreRender(Shader& shader, float deltaTime) override { }

    /*
        Can: Render objects, access render data of objects, assign object ubo's, assign transforms to objects (before drawing them), run calculations on objects
        Cannot: Create objects
    */
    void OnPostRender(Shader& shader, float deltaTime) override {
        for (auto el : objects) {
            std::string id = el.first;
            Object object = el.second;

            object.model->draw(shader, object.modelTransform);
        }
    }

    Camera& getCamera() { return camera; }

private:
    std::unordered_map<std::string, Object> objects;
    Camera camera;

    float carSpeed = 5.0f;
    float rotationSpeed = 3.0f; // radians per second
};