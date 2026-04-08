#pragma once

#include "assetManager.hpp"
#include "sceneManager.hpp"

struct RigidBody;

struct Object {
    ModelResource* model;
    Transform      modelTransform;
    RigidBody*     rigidBody = nullptr;
};