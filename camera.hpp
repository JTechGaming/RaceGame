#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "dynamicObject.hpp"

struct AngleLimits {
    float pitch;
    float yaw;
};

struct ZoomLimits {
    float near;
    float far;
};

class Camera {
public:
    Camera() {

    }

    void attachToObject(Object* object, glm::vec3 offset, float radius, float anglePitch, float angleYaw, AngleLimits angleLimits, ZoomLimits zoomLimits) {
        parentObject = object;
        this->offset = offset;
        this->radius = radius;
        this->anglePitch = anglePitch;
        this->angleYaw = angleYaw;
        this->angleLimits = angleLimits;
        this->zoomLimits = zoomLimits;
        pitch = anglePitch;
        yaw = angleYaw;
    }

    void updateOrbitRadius(float newRadius) {
        radius = std::min(std::max(newRadius, zoomLimits.near), zoomLimits.far);
    }

    void tick() {
        if (parentObject) {
            glm::vec3 target = parentObject->modelTransform.pos + offset;
            
            glm::vec3 direction;
            direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
            direction.y = sin(glm::radians(pitch));
            direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
            
            cameraPos = target + radius * direction;
            cameraFront = -glm::normalize(direction);
        }
    }

    void processMouse(double xpos, double ypos) {
        if (firstMouse) {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }
        
        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos;
        lastX = xpos;
        lastY = ypos;

        const float sensitivity = 0.1f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        yaw += xoffset;
        pitch -= yoffset;

        if(pitch > angleLimits.pitch)
            pitch = angleLimits.pitch;
        if(pitch < -angleLimits.pitch)
            pitch = -angleLimits.pitch;
    }

    // Getters
    glm::vec3 getCameraPos() const { return cameraPos; }
    glm::vec3 getCameraFront() const { return cameraFront; }
    glm::vec3 getCameraUp() const { return cameraUp; }
    float getFov() const { return fov; }

private:
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);

    Object* parentObject;

    glm::vec3 offset;
    float radius;
    float anglePitch, angleYaw;
    AngleLimits angleLimits;
    ZoomLimits zoomLimits;

    float fov = 90.0f;

    float lastX = 400, lastY = 300;
    float pitch = 0.0f, yaw = 0.0f;
    bool firstMouse = true;
};