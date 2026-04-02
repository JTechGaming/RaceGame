#pragma once

#include "IGame.hpp"
#include "transform.hpp"
#include "camera.hpp"
#include "physicsEngine.hpp"

#include "mathUtils.hpp"

#include <glm/gtx/euler_angles.hpp>

struct CarInput {
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
    bool brake = false;
};

void buildTrackGrid(TrackCollider& track) {
    if(track.triangles.empty())
        return;

    track.minBound = track.triangles[0].v0;
    track.maxBound = track.triangles[0].v0;

    for(const Triangle& tri : track.triangles)
    {
        for(glm::vec3 v : {tri.v0, tri.v1, tri.v2})
        {
            track.minBound = glm::min(track.minBound, v);
            track.maxBound = glm::max(track.maxBound, v);
        }
    }

    track.gridWidth = (track.maxBound.x - track.minBound.x) / track.cellSize + 1;
    track.gridDepth = (track.maxBound.z - track.minBound.z) / track.cellSize + 1;

    track.grid.resize(track.gridWidth * track.gridDepth);

    for(int i = 0; i < track.triangles.size(); i++)
    {
        const Triangle& tri = track.triangles[i];

        float minX = std::min({tri.v0.x, tri.v1.x, tri.v2.x});
        float maxX = std::max({tri.v0.x, tri.v1.x, tri.v2.x});
        float minZ = std::min({tri.v0.z, tri.v1.z, tri.v2.z});
        float maxZ = std::max({tri.v0.z, tri.v1.z, tri.v2.z});

        int gx0 = (minX - track.minBound.x) / track.cellSize;
        int gx1 = (maxX - track.minBound.x) / track.cellSize;
        int gz0 = (minZ - track.minBound.z) / track.cellSize;
        int gz1 = (maxZ - track.minBound.z) / track.cellSize;

        for(int x = gx0; x <= gx1; x++)
        for(int z = gz0; z <= gz1; z++)
        {
            int index = x + z * track.gridWidth;
            track.grid[index].push_back(i);
        }
    }
}

TrackCollider buildTrackCollider(ModelResource* model, const Transform& transform) {
    TrackCollider collider;

    glm::mat4 modelMatrix =
        glm::translate(glm::mat4(1.0f), transform.pos) *
        glm::yawPitchRoll(
            glm::radians(transform.rot.y),
            glm::radians(transform.rot.x),
            glm::radians(transform.rot.z)) *
        glm::scale(glm::mat4(1.0f), transform.scale);

    for (const Mesh& mesh : *model->getMeshes())
    {
        for (size_t i = 0; i < mesh.indices.size(); i += 3)
        {
            const Vertex& v0 = mesh.vertices[mesh.indices[i]];
            const Vertex& v1 = mesh.vertices[mesh.indices[i+1]];
            const Vertex& v2 = mesh.vertices[mesh.indices[i+2]];

            Triangle tri;

            tri.v0 = glm::vec3(modelMatrix * glm::vec4(v0.position,1));
            tri.v1 = glm::vec3(modelMatrix * glm::vec4(v1.position,1));
            tri.v2 = glm::vec3(modelMatrix * glm::vec4(v2.position,1));

            tri.normal = glm::normalize(glm::cross(tri.v1-tri.v0,
                                                   tri.v2-tri.v0));

            collider.triangles.push_back(tri);
        }
    }

    buildTrackGrid(collider);

    return collider;
}

class RaceGame : public IGame {
public:
    void OnInit(AssetManager* assetManager, SceneManager* sceneManager, std::string basePath) override {
        m_assetManager = assetManager;
        m_sceneManager = sceneManager;

        objects.reserve(3);

        Object car{};
        car.model = assetManager->loadModel(AssetManager::buildModelPath("models/car2"));
        car.modelTransform = Transform{glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f), glm::vec3(1.0f)};
        objects.insert({"Player", car});

        RigidBody* playerBody = new RigidBody(glm::vec3(0, 10, 0), glm::vec3(1.0f, 0.5f, 2.0f), 1200.0f);
        physicsEngine.addBody(playerBody);
        bodyMap["Player"] = playerBody;

        camera.attachToObject(&objects["Player"], glm::vec3(0.0f, 2.0f, 0.0f), 5.0f, -15.0f, 0.0f, {45.0f, 360.0f}, {2.0f, 10.0f});

        trackModel = assetManager->loadModel(
            AssetManager::buildModelPath("models/track")
        );

        Transform trackTransform;

        for (const SceneObject& obj : m_sceneManager->getScene().sceneObjects) {
            if (obj.modelPath == "models/track")
            {
                trackTransform = obj.transform;
                break;
            }
        }

        trackCollider = buildTrackCollider(trackModel, trackTransform);

        printf("Track triangles: %zu\n", trackCollider.triangles.size());

        RacingVehicle playerVehicle;
        playerVehicle.body = playerBody;
        playerVehicle.wheels = {
            {{-0.9f,0.0f, 1.3f}},
            {{ 0.9f,0.0f, 1.3f}},
            {{-0.9f,0.0f,-1.3f}},
            {{ 0.9f,0.0f,-1.3f}}
        };
        vehicles["Player"] = playerVehicle;

        std::vector<std::string> others = {"Fake1", "Fake2"};
        std::vector<glm::vec3> positions = {glm::vec3(3, 10, 0), glm::vec3(-3, 10, 0)};

        for (int i = 0; i < (int)others.size(); i++) {
            Object otherCar{};
            otherCar.model = assetManager->loadModel(AssetManager::buildModelPath("models/car2"));
            otherCar.modelTransform = Transform{positions[i], glm::vec3(0.0f), glm::vec3(1.0f)};
            objects.insert({others[i], otherCar});

            RigidBody* rb = new RigidBody(positions[i], glm::vec3(1.0f, 0.5f, 2.0f), 1200.0f);
            physicsEngine.addBody(rb);
            bodyMap[others[i]] = rb;

            RacingVehicle botVehicle;
            botVehicle.body = rb;
            botVehicle.wheels = {
                {{-0.9f,0.0f, 1.3f}},
                {{ 0.9f,0.0f, 1.3f}},
                {{-0.9f,0.0f,-1.3f}},
                {{ 0.9f,0.0f,-1.3f}}
            };
            vehicles[others[i]] = botVehicle;
        }

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
            }

            glm::mat3 rotationMat = glm::mat3_cast(rb->m_orientation);
            glm::vec3 forward = rotationMat[2];
            glm::vec3 right   = rotationMat[0];
            glm::vec3 up      = rotationMat[1];

            float speed = glm::length(rb->m_velocity);

            // --- Physics Config ---
            const float engineForce = 45000.0f;
            const float maxSpeed = 85.0f;
            const float baseTurnTorque = 28000.0f;
            const float dragCoeff = 0.001f;
            const float downforceCoeff = 45.0f;

            float forwardSpeed = glm::dot(rb->m_velocity, forward);
            float lateralSpeed = glm::dot(rb->m_velocity, right);
            float speedRatio = std::abs(forwardSpeed) / maxSpeed;

            if (isKeyPressed(GLFW_KEY_UP)) {
                rb->m_velocity.y += 0.2f;
            }

            if (rb->m_onGround) {
                float downforce = glm::clamp(speed * speed * downforceCoeff, 0.0f, 80000.0f);

                rb->m_velocity -= up * downforce * rb->m_inverseMass * deltaTime;

                // dynamic steering (Scales with speed)
                float steerAbility = glm::clamp(1.0f - (speedRatio * 0.7f), 0.2f, 1.0f);
                float steerInput = 0.0f;
                if (input.left) steerInput = 1.0f;
                if (input.right) steerInput = -1.0f;

                rb->m_angularVelocity *= 0.98f;

                float currentTurnTorque = steerInput * baseTurnTorque * steerAbility;
                rb->m_angularVelocity += up * currentTurnTorque * rb->m_inertiaTensorInv[1][1] * deltaTime;

                // Traction and grip
                float slipAngle = std::abs(lateralSpeed) / (std::abs(forwardSpeed) + 0.1f);
                bool isDrifting = input.brake && std::abs(forwardSpeed) > 15.0f;

                // Grip factor: the lower, the more sidewards slide
                float gripFactor = isDrifting ? 0.12f : 0.98f; 

                // lateral friction slows down sidewards sliding
                float frictionImpulse = -lateralSpeed * gripFactor;
                rb->m_velocity += right * frictionImpulse * 8.0f * deltaTime;

                // Centrifugal acceleration while drifting
                if (isDrifting || slipAngle > 0.3f) {
                    // We project a portion of the lateral velocity back into the forward direction.
                    // The sharper the nose is positioned, the harder the car is "pulled" in that direction.
                    float pushIntoTurn = std::abs(lateralSpeed) * 0.6f; 
                    rb->m_velocity += forward * pushIntoTurn * deltaTime;

                    // Extra rotation assistance during drift (oversteer simulation)
                    float oversteer = (input.left ? 1.0f : (input.right ? -1.0f : 0.0f));
                    rb->m_angularVelocity += up * oversteer * baseTurnTorque * 0.5f * rb->m_inertiaTensorInv[1][1] * deltaTime;
                }

                // speed loss (Increased during drift for balance)
                float driftResist = isDrifting ? 0.4f : 0.05f;
                rb->m_velocity -= forward * forwardSpeed * driftResist * deltaTime;

                // acceleration
                if (input.forward && forwardSpeed < maxSpeed) {
                    // Less power when you have wheelspin (high slip angle)
                    float tractionControl = glm::clamp(1.0f - slipAngle, 0.4f, 1.0f);
                    rb->m_velocity += forward * engineForce * tractionControl * rb->m_inverseMass * deltaTime;
                }

                // Braking
                if (input.brake) {
                    rb->m_velocity -= forward * forwardSpeed * 0.8f * deltaTime;
                }
            }

            // Air resistance & natural damping
            rb->m_velocity *= (1.0f - dragCoeff);
            rb->m_angularVelocity *= 0.95f;
        }

        for (auto& [id, vehicle] : vehicles) {
            vehicle.updateSuspension(deltaTime, trackCollider);
        }

        physicsEngine.step(deltaTime);

        for (auto const& [id, body] : bodyMap) {
            objects[id].modelTransform.pos = body->m_position;
            objects[id].modelTransform.rot = glm::eulerAngles(body->m_orientation);
        }
    }

    void OnPreRender(Shader& shader, float deltaTime) override {}

    void OnPostRender(Shader& shader, float deltaTime) override {
        for (auto& el : objects)
            el.second.model->draw(shader, el.second.modelTransform);

        if (m_debug) {
            for (glm::vec3 point : m_sceneManager->getScene().trackSpline._points) {
                shader.setVec3("material.baseColor", glm::vec3(1.0f, 0.0f, 0.0f));
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

    bool m_isDrifting = false;
    ModelResource* trackModel = nullptr;
    TrackCollider trackCollider;
};