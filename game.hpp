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
        car.modelTransform = Transform{glm::vec3(0.0f, -2.0f, 0.0f), glm::vec3(0.0f), glm::vec3(1.0f)};
        objects.insert({"Player", car});

        glm::vec3 playerDimensions = MathUtils::findModelDimensions(car.model) / 2.0f;
        RigidBody* playerBody = new RigidBody(glm::vec3(0, playerDimensions.y, 0), playerDimensions, 1200.0f);
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
            {{-0.85f,-0.25f, 1.1f}},
            {{ 0.85f,-0.25f, 1.1f}},
            {{-0.85f,-0.25f,-1.45f}},
            {{ 0.85f,-0.25f,-1.45f}}
        };
        vehicles["Player"] = playerVehicle;

        std::vector<std::string> others = {"Fake1", "Fake2"};
        std::vector<glm::vec3> positions = {glm::vec3(3, 1, 0), glm::vec3(-3, 1, 0)};

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
                {{-0.85f,-0.25f, 1.1f}},
                {{ 0.85f,-0.25f, 1.1f}},
                {{-0.85f,-0.25f,-1.45f}},
                {{ 0.85f,-0.25f,-1.45f}}
            };
            vehicles[others[i]] = botVehicle;
        }

        debugCube = m_assetManager->loadModel(AssetManager::buildModelPath("models/cube"));
    }

    void OnShutdown() override {}

    void OnTick(float deltaTime) override {
        const float maxSpeed = 150.0f;
        const float maxReverseSpeed = 18.0f;
        const float accel = 40.0f;
        const float reverseAccel = 24.0f;
        const float brakeDamping = 4.2f;
        const float coastDamping = 1.35f;
        const float steerRate = 20.0f;
        const float rideHeight = 0.58f;
        const float wallBounce = 0.45f;
        const float wallPushback = 0.2f;
        const float wallProbeDist = 1.8f;
        const float wallNormalYThreshold = 0.08f;
        const float curbHeightIgnore = 0.12f;
        const float wallClearanceForward = 1.05f;
        const float wallClearanceSide = 0.82f;
        const float wallProbeHalfWidth = 0.78f;
        const float orientationSmooth = 8.0f;
        const float heightSmooth = 10.0f;
        const float groundNormalSmooth = 10.0f;
        const float groundSnapGrace = 0.16f;
        const float maxStepDown = 0.45f;
        const float recoveryMaxStepDown = 3.0f;
        const float fallbackDescentSpeed = 7.0f;

        std::vector<std::string> carIds = {"Player", "Fake1", "Fake2"};

        for (const std::string& id : carIds) {
            RigidBody* rb = bodyMap[id];
            CarInput input;

            GroundState& groundState = m_groundStates[id];

            if (id == "Player") {
                input.forward  = isKeyPressed(GLFW_KEY_W);
                input.backward = isKeyPressed(GLFW_KEY_S);
                input.left     = isKeyPressed(GLFW_KEY_A);
                input.right    = isKeyPressed(GLFW_KEY_D);
                input.brake    = isKeyPressed(GLFW_KEY_SPACE);
            }

            glm::mat3 rotationMat = glm::mat3_cast(rb->m_orientation);
            glm::vec3 carForward = glm::normalize(rotationMat[2]);
            glm::vec3 carRight = glm::normalize(rotationMat[0]);
            glm::vec3 carUp = glm::normalize(rotationMat[1]);
            float speed = m_forwardSpeed[id];
            float dynamicStepDown = maxStepDown + std::abs(speed) * deltaTime * 2.0f;

            if (!groundState.hasGround) {
                groundState.hasGround = true;
                groundState.groundY = rb->m_position.y - rideHeight;
                groundState.normal = carUp;
            }

            float groundY = groundState.groundY;
            glm::vec3 groundNormal = groundState.normal;
            bool isGrounded = sampleGround(
                rb->m_position,
                carRight,
                carForward,
                groundState.groundY,
                carUp,
                dynamicStepDown,
                groundY,
                groundNormal);

            if (isGrounded) {
                float nAlpha = glm::clamp(groundNormalSmooth * deltaTime, 0.0f, 1.0f);
                glm::vec3 blendedNormal = glm::normalize(glm::mix(groundState.normal, groundNormal, nAlpha));

                groundState.hasGround = true;
                groundState.groundY = groundY;
                groundState.normal = blendedNormal;
                groundState.airborneTime = 0.0f;

                groundNormal = blendedNormal;
            } else {
                groundState.airborneTime += deltaTime;
                if (groundState.airborneTime < groundSnapGrace) {
                    isGrounded = true;
                    groundY = groundState.groundY;
                    groundNormal = groundState.normal;
                } else {
                    // Recovery probe: allow larger drop to reacquire stair/ramp transitions.
                    float recoveredY = groundY;
                    glm::vec3 recoveredNormal = groundNormal;
                    bool recovered = sampleGround(
                        rb->m_position,
                        carRight,
                        carForward,
                        rb->m_position.y - rideHeight,
                        carUp,
                        recoveryMaxStepDown,
                        recoveredY,
                        recoveredNormal);

                    if (recovered) {
                        float nAlpha = glm::clamp((groundNormalSmooth * 0.7f) * deltaTime, 0.0f, 1.0f);
                        glm::vec3 blendedNormal = glm::normalize(glm::mix(groundState.normal, recoveredNormal, nAlpha));

                        isGrounded = true;
                        groundY = recoveredY;
                        groundNormal = blendedNormal;
                        groundState.groundY = recoveredY;
                        groundState.normal = blendedNormal;
                        groundState.airborneTime = 0.0f;
                    }
                }
            }

            rb->m_onGround = isGrounded;

            glm::vec3 forwardOnGround = carForward - groundNormal * glm::dot(carForward, groundNormal);
            if (glm::length(forwardOnGround) < 0.001f)
                forwardOnGround = glm::vec3(0.0f, 0.0f, 1.0f);
            else
                forwardOnGround = glm::normalize(forwardOnGround);

            float steerInput = 0.0f;
            bool isGoingBackwards = speed < -0.5f;
            if (input.left && !isGoingBackwards) steerInput += 1.0f;
            if (input.right && !isGoingBackwards) steerInput -= 1.0f;
            if (input.left && isGoingBackwards) steerInput -= 1.0f;
            if (input.right && isGoingBackwards) steerInput += 1.0f;

            float steerScale = glm::clamp(std::abs(speed) / maxSpeed, 0.25f, 1.0f);
            float steerAmount = steerInput * steerRate * steerScale * deltaTime;
            if (std::abs(steerAmount) > 0.0f) {
                forwardOnGround = glm::normalize(glm::rotate(
                    glm::angleAxis(steerAmount, groundNormal),
                    forwardOnGround));
            }

            if (input.forward) speed += accel * deltaTime;
            if (input.backward) speed -= reverseAccel * deltaTime;
            if (input.brake) speed *= glm::max(0.0f, 1.0f - brakeDamping * deltaTime);
            speed *= glm::max(0.0f, 1.0f - coastDamping * deltaTime);
            speed = glm::clamp(speed, -maxReverseSpeed, maxSpeed);

            glm::vec3 candidatePos = rb->m_position + forwardOnGround * speed * deltaTime;

            float snappedGroundY = groundY;
            glm::vec3 snappedNormal = groundNormal;
            if (sampleGround(
                    candidatePos + groundNormal * 0.25f,
                    carRight,
                    forwardOnGround,
                    groundState.groundY,
                    carUp,
                    dynamicStepDown,
                    snappedGroundY,
                    snappedNormal)) {
                float targetY = snappedGroundY + rideHeight;
                float hAlpha = glm::clamp(heightSmooth * deltaTime, 0.0f, 1.0f);
                candidatePos.y = glm::mix(rb->m_position.y, targetY, hAlpha);

                groundState.groundY = snappedGroundY;
                groundState.normal = snappedNormal;
                groundState.airborneTime = 0.0f;
            } else {
                // Relaxed reacquisition for fast downward transitions (stairs).
                float recoveredY = snappedGroundY;
                glm::vec3 recoveredNormal = snappedNormal;
                bool recovered = sampleGround(
                    candidatePos,
                    carRight,
                    forwardOnGround,
                    candidatePos.y - rideHeight,
                    carUp,
                    recoveryMaxStepDown,
                    recoveredY,
                    recoveredNormal);

                if (recovered) {
                    float targetY = recoveredY + rideHeight;
                    float hAlpha = glm::clamp((heightSmooth * 0.7f) * deltaTime, 0.0f, 1.0f);
                    candidatePos.y = glm::mix(rb->m_position.y, targetY, hAlpha);
                    snappedNormal = recoveredNormal;

                    groundState.groundY = recoveredY;
                    groundState.normal = recoveredNormal;
                    groundState.airborneTime = 0.0f;
                } else {
                    // Never stay suspended forever if no valid ground was found this frame.
                    candidatePos.y = rb->m_position.y - fallbackDescentSpeed * deltaTime;
                    snappedNormal = groundNormal;
                }
            }

            glm::vec3 wallProbeRight = glm::cross(snappedNormal, forwardOnGround);
            if (glm::length(wallProbeRight) < 0.001f)
                wallProbeRight = glm::vec3(1.0f, 0.0f, 0.0f);
            else
                wallProbeRight = glm::normalize(wallProbeRight);

            auto applyWallProbe = [&](const glm::vec3& rayStart,
                                      const glm::vec3& rayDir,
                                      float rayDist,
                                      float desiredClearance) {
                RaycastHit hit = raycastTrack(rayStart, rayDir, rayDist, trackCollider);
                if (!hit.hit)
                    return;

                glm::vec3 wallNormal = glm::normalize(hit.normal);
                if (glm::dot(wallNormal, rayDir) > 0.0f)
                    wallNormal = -wallNormal;

                bool isVerticalSurface = std::abs(wallNormal.y) <= wallNormalYThreshold;
                bool differsFromGround = std::abs(glm::dot(wallNormal, snappedNormal)) < 0.45f;
                if (!isVerticalSurface || !differsFromGround)
                    return;

                glm::vec3 hitPoint = rayStart + rayDir * hit.distance;

                float aheadGroundY = snappedGroundY;
                glm::vec3 aheadGroundNormal = snappedNormal;
                bool hasAheadGround = sampleGround(
                    hitPoint + rayDir * 0.35f,
                    wallProbeRight,
                    forwardOnGround,
                    snappedGroundY,
                    carUp,
                    maxStepDown,
                    aheadGroundY,
                    aheadGroundNormal);

                bool looksLikeSmallCurb = hasAheadGround && std::abs(aheadGroundY - snappedGroundY) <= curbHeightIgnore;
                if (looksLikeSmallCurb)
                    return;

                float penetration = desiredClearance - hit.distance;
                if (penetration <= 0.0f)
                    return;

                candidatePos += wallNormal * (penetration + wallPushback * 0.25f);

                float approach = glm::dot(forwardOnGround, wallNormal);
                if (approach < 0.0f) {
                    speed *= glm::clamp(1.0f - (-approach * wallBounce), 0.0f, 1.0f);
                }
            };

            float dynamicWallProbeDist = wallProbeDist + std::abs(speed) * deltaTime;
            glm::vec3 probeUpOffset = snappedNormal * 0.25f;

            // Front center/left/right probes make head-on wall detection reliable.
            applyWallProbe(candidatePos + probeUpOffset, forwardOnGround, dynamicWallProbeDist, wallClearanceForward);
            applyWallProbe(candidatePos + wallProbeRight * wallProbeHalfWidth + probeUpOffset, forwardOnGround, dynamicWallProbeDist, wallClearanceForward);
            applyWallProbe(candidatePos - wallProbeRight * wallProbeHalfWidth + probeUpOffset, forwardOnGround, dynamicWallProbeDist, wallClearanceForward);

            // Side probes prevent slipping through walls while sliding/turning near them.
            applyWallProbe(candidatePos + probeUpOffset, wallProbeRight, wallClearanceSide + 0.25f, wallClearanceSide);
            applyWallProbe(candidatePos + probeUpOffset, -wallProbeRight, wallClearanceSide + 0.25f, wallClearanceSide);

            glm::vec3 rightOnGround = glm::cross(snappedNormal, forwardOnGround);
            if (glm::length(rightOnGround) < 0.001f)
                rightOnGround = glm::vec3(1.0f, 0.0f, 0.0f);
            else
                rightOnGround = glm::normalize(rightOnGround);

            auto sampleWheelGroundY = [&](const glm::vec3& probePos, float& outY) {
                glm::vec3 hitNormal = snappedNormal;
                return sampleGround(
                    probePos + snappedNormal * 0.2f,
                    rightOnGround,
                    forwardOnGround,
                    snappedGroundY,
                    carUp,
                    recoveryMaxStepDown,
                    outY,
                    hitNormal);
            };

            float frontOffset = 1.3f;
            float rearOffset = -1.3f;
            float halfTrack = 0.9f;
            auto vehicleIt = vehicles.find(id);
            if (vehicleIt != vehicles.end() && !vehicleIt->second.wheels.empty()) {
                frontOffset = -1000.0f;
                rearOffset = 1000.0f;
                halfTrack = 0.0f;

                for (const Wheel& wheel : vehicleIt->second.wheels) {
                    frontOffset = std::max(frontOffset, wheel.connectionPoint.z);
                    rearOffset = std::min(rearOffset, wheel.connectionPoint.z);
                    halfTrack = std::max(halfTrack, std::abs(wheel.connectionPoint.x));
                }
            }

            float flY = 0.0f, frY = 0.0f, rlY = 0.0f, rrY = 0.0f;
            bool hasFL = sampleWheelGroundY(candidatePos + forwardOnGround * frontOffset - rightOnGround * halfTrack, flY);
            bool hasFR = sampleWheelGroundY(candidatePos + forwardOnGround * frontOffset + rightOnGround * halfTrack, frY);
            bool hasRL = sampleWheelGroundY(candidatePos + forwardOnGround * rearOffset - rightOnGround * halfTrack, rlY);
            bool hasRR = sampleWheelGroundY(candidatePos + forwardOnGround * rearOffset + rightOnGround * halfTrack, rrY);

            float frontY = snappedGroundY;
            int frontHits = 0;
            if (hasFL) { frontY += flY; frontHits++; }
            if (hasFR) { frontY += frY; frontHits++; }
            if (frontHits > 0)
                frontY /= (float)(frontHits + 1);

            float rearY = snappedGroundY;
            int rearHits = 0;
            if (hasRL) { rearY += rlY; rearHits++; }
            if (hasRR) { rearY += rrY; rearHits++; }
            if (rearHits > 0)
                rearY /= (float)(rearHits + 1);

            float wheelGroundSum = 0.0f;
            int wheelGroundHits = 0;
            if (hasFL) { wheelGroundSum += flY; wheelGroundHits++; }
            if (hasFR) { wheelGroundSum += frY; wheelGroundHits++; }
            if (hasRL) { wheelGroundSum += rlY; wheelGroundHits++; }
            if (hasRR) { wheelGroundSum += rrY; wheelGroundHits++; }
            if (wheelGroundHits > 0) {
                float targetY = (wheelGroundSum / (float)wheelGroundHits) + rideHeight;
                float hAlpha = glm::clamp(heightSmooth * deltaTime, 0.0f, 1.0f);
                candidatePos.y = glm::mix(candidatePos.y, targetY, hAlpha);
            }

            float wheelBase = glm::max(frontOffset - rearOffset, 0.2f);
            float slope = glm::clamp((frontY - rearY) / wheelBase, -1.0f, 1.0f);

            // Hard no-roll safeguard: always build orientation from world up and pitch only.
            glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
            glm::vec3 yawForward = glm::vec3(forwardOnGround.x, 0.0f, forwardOnGround.z);
            if (glm::length(yawForward) < 0.001f)
                yawForward = glm::vec3(0.0f, 0.0f, 1.0f);
            else
                yawForward = glm::normalize(yawForward);

            glm::vec3 pitchedForward = glm::normalize(yawForward + worldUp * slope);
            glm::vec3 right = glm::cross(worldUp, pitchedForward);
            if (glm::length(right) < 0.001f)
                right = glm::vec3(1.0f, 0.0f, 0.0f);
            else
                right = glm::normalize(right);

            glm::vec3 up = glm::normalize(glm::cross(pitchedForward, right));

            glm::mat3 targetRotation(right, up, pitchedForward);
            glm::quat targetOrientation = glm::normalize(glm::quat_cast(targetRotation));
            float rotAlpha = glm::clamp(orientationSmooth * deltaTime, 0.0f, 1.0f);

            rb->m_position = candidatePos;
            rb->m_orientation = glm::normalize(glm::slerp(rb->m_orientation, targetOrientation, rotAlpha));
            rb->m_velocity = pitchedForward * speed;
            rb->m_angularVelocity = glm::vec3(0.0f);
            rb->updateTransform();

            m_forwardSpeed[id] = speed;
        }

        for (auto const& [id, body] : bodyMap) {
            objects[id].modelTransform.pos = body->m_position - glm::vec3(0.0f, body->m_obb.halfExtents.y, 0.0f);
            objects[id].modelTransform.rot = glm::eulerAngles(body->m_orientation);
        }
    }

    void OnPreRender(Shader& shader, float deltaTime) override {}

    void OnPostRender(Shader& shader, float deltaTime) override {
        for (auto& el : objects)
            el.second.model->draw(shader, el.second.modelTransform);

        if (m_wireframe) {
            for (auto& wheel : vehicles["Player"].wheels) {
                glm::vec3 worldWheelPos = vehicles["Player"].body->m_position +
                                        vehicles["Player"].body->m_obb.rotation * wheel.connectionPoint;
                Transform debugTransform{worldWheelPos, glm::vec3(0.0f), glm::vec3(0.1f)};
                shader.setVec3("material.baseColor", glm::vec3(0.0f, 1.0f, 0.0f));
                debugCube->draw(shader, debugTransform);
            }

            Transform carRidgidBodyTransform{vehicles["Player"].body->m_obb.center, glm::eulerAngles(vehicles["Player"].body->m_orientation), vehicles["Player"].body->m_obb.halfExtents};
            shader.setVec3("material.baseColor", glm::vec3(0.0f, 0.0f, 1.0f));
            debugCube->draw(shader, carRidgidBodyTransform);
        }

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
    void setWireframe(bool wireframe) override { m_wireframe = wireframe; }
    bool getWireframe() const override { return m_wireframe; }

    Camera& getCamera() { return camera; }

private:
    struct GroundState {
        bool hasGround = false;
        float groundY = 0.0f;
        glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
        float airborneTime = 0.0f;
    };

    bool sampleGround(const glm::vec3& probePos,
                      const glm::vec3& rightDir,
                      const glm::vec3& forwardDir,
                      float expectedGroundY,
                      const glm::vec3& preferredUp,
                      float maxStepDown,
                      float& outGroundY,
                      glm::vec3& outNormal) const {
        const glm::vec3 down(0.0f, -1.0f, 0.0f);
        const float localStartOffset = 1.1f;
        const float localProbeDist = 3.2f;
        const float minGroundNormalY = 0.05f;

        const glm::vec3 right = glm::normalize(rightDir);
        const glm::vec3 forward = glm::normalize(forwardDir);
        const glm::vec3 offsets[] = {
            glm::vec3(0.0f),
            right * 0.7f + forward * 1.0f,
            -right * 0.7f + forward * 1.0f,
            right * 0.7f - forward * 1.0f,
            -right * 0.7f - forward * 1.0f
        };

        bool found = false;
        float bestScore = FLT_MAX;
        float bestY = expectedGroundY;
        glm::vec3 bestNormal = preferredUp;

        for (const glm::vec3& offset : offsets) {
            glm::vec3 rayStart = probePos + offset + glm::vec3(0.0f, localStartOffset, 0.0f);
            RaycastHit hit = raycastTrack(rayStart, down, localProbeDist, trackCollider);
            if (!hit.hit)
                continue;

            glm::vec3 n = glm::normalize(hit.normal);
            if (glm::dot(n, preferredUp) < 0.0f)
                n = -n;

            if (n.y < minGroundNormalY)
                continue;

            float y = rayStart.y - hit.distance;
            if (y < expectedGroundY - maxStepDown)
                continue;

            float score = std::abs(y - expectedGroundY);
            if (!found || score < bestScore) {
                found = true;
                bestScore = score;
                bestY = y;
                bestNormal = n;
            }
        }

        if (!found)
            return false;

        outGroundY = bestY;
        outNormal = glm::normalize(bestNormal);
        return true;
    }

    AssetManager* m_assetManager;
    SceneManager* m_sceneManager;

    std::unordered_map<std::string, Object> objects;
    std::unordered_map<std::string, RigidBody*> bodyMap;
    SimplePhysicsEngine physicsEngine;
    Camera camera;
    bool m_debug = false;
    bool m_wireframe = false;

    ModelResource* debugCube = nullptr;

    uint16_t bot1NextSplineIndex = 0;
    uint16_t bot2NextSplineIndex = 0;

    std::unordered_map<std::string, RacingVehicle> vehicles;
    std::unordered_map<std::string, float> m_forwardSpeed;
    std::unordered_map<std::string, GroundState> m_groundStates;

    bool m_isDrifting = false;
    ModelResource* trackModel = nullptr;
    TrackCollider trackCollider;
};