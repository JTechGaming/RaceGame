#pragma once

#include "assetManager.hpp"
#include "sceneManager.hpp"
#include "shader.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

class IGame {
public:
    virtual void OnInit(AssetManager* assetManager, SceneManager* sceneManager, std::string basePath) = 0;
    virtual void OnShutdown() = 0;

    virtual void OnTick(float deltaTime) = 0;
    virtual void OnPreRender(Shader& shader, float deltaTime) = 0;
    virtual void OnPostRender(Shader& shader, float deltaTime) = 0;

    virtual void setDebug(bool debug) = 0;
    virtual bool getDebug() const = 0;
    virtual void setWireframe(bool wireframe) = 0;
    virtual bool getWireframe() const = 0;

    // Input helper functions
    void setWindow(GLFWwindow* window) { m_window = window; }

    bool isKeyPressed(int key) const {
        if (m_window == nullptr) return false;
        return glfwGetKey(m_window, key) == GLFW_PRESS;
    }

    glm::vec3 moveForward(glm::vec3 pos, glm::vec3 direction, float speed, float deltaTime) const {
        return pos + direction * speed * deltaTime;
    }

    glm::vec3 moveBackward(glm::vec3 pos, glm::vec3 direction, float speed, float deltaTime) const {
        return pos - direction * speed * deltaTime;
    }

    glm::vec3 strafeLeft(glm::vec3 pos, glm::vec3 direction, glm::vec3 up, float speed, float deltaTime) const {
        return pos - glm::normalize(glm::cross(direction, up)) * speed * deltaTime;
    }

    glm::vec3 strafeRight(glm::vec3 pos, glm::vec3 direction, glm::vec3 up, float speed, float deltaTime) const {
        return pos + glm::normalize(glm::cross(direction, up)) * speed * deltaTime;
    }

    glm::vec3 moveUp(glm::vec3 pos, glm::vec3 up, float speed, float deltaTime) const {
        return pos + up * speed * deltaTime;
    }

    glm::vec3 moveDown(glm::vec3 pos, glm::vec3 up, float speed, float deltaTime) const {
        return pos - up * speed * deltaTime;
    }

private:
    GLFWwindow* m_window = nullptr;
};