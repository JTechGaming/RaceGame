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
        RigidBody* playerBody = new RigidBody(glm::vec3(0, 7.0f, 0), playerDimensions, 1200.0f);
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
        std::vector<glm::vec3> positions = {glm::vec3(3, 7, 0), glm::vec3(-3, 7, 0)};

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
        const float steerRate = 55.0f;
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
            if (id == "Player") {
                input.forward  = isKeyPressed(GLFW_KEY_W);
                input.backward = isKeyPressed(GLFW_KEY_S);
                input.left     = isKeyPressed(GLFW_KEY_A);
                input.right    = isKeyPressed(GLFW_KEY_D);
                input.brake    = isKeyPressed(GLFW_KEY_SPACE);
            }

            auto& vehicle = vehicles[id];
            glm::vec3 carPos = rb->m_position;
            glm::quat carRot = rb->m_orientation;
            float speed = m_forwardSpeed[id];
            float wheelRadius = 0.3f;
            float raycastHeight = 2.0f;
            glm::mat3 rotMat = glm::mat3_cast(carRot);

            // --- MOVEMENT LOGIC (input, speed, steering) ---
            // Extract heading as flat XZ direction — pitch/roll come only from ground alignment
            glm::vec3 carForward = rotMat[2];
            glm::vec3 carRight = rotMat[0];
            // Flatten to pure heading so previous frame's tilt can't contaminate steering
            carForward = glm::vec3(carForward.x, 0.0f, carForward.z);
            if (glm::length(carForward) < 0.001f) carForward = glm::vec3(0, 0, 1);
            carForward = glm::normalize(carForward);

            // --- DRIFT PARAMETERS ---
            const float driftEntrySpeed = 15.0f;      // min speed to start drifting
            const float driftSteerMultiplier = 2.2f;   // how much faster the car rotates while drifting
            const float driftFriction = 0.45f;          // lateral friction during drift (lower = more slide)
            const float driftRecoveryRate = 3.0f;      // how fast velocity realigns after drift ends
            const float maxDriftAngle = 1.2f;          // max slip angle in radians (~69 degrees)

            DriftState& drift = m_driftStates[id];

            // Steering
            float steerInput = 0.0f;
            bool isGoingBackwards = speed < -0.5f;
            if (input.left && !isGoingBackwards) steerInput += 1.0f;
            if (input.right && !isGoingBackwards) steerInput -= 1.0f;
            if (input.left && isGoingBackwards) steerInput -= 1.0f;
            if (input.right && isGoingBackwards) steerInput += 1.0f;

            // Drift entry: brake + steer + going fast enough
            bool wantsDrift = input.brake && std::abs(steerInput) > 0.01f && std::abs(speed) > driftEntrySpeed;
            if (wantsDrift) {
                drift.isDrifting = true;
            }
            if (drift.isDrifting && (std::abs(speed) < 3.0f || (!input.brake && std::abs(drift.driftAngle) < 0.05f))) {
                drift.isDrifting = false;
            }

            float steerScale = glm::clamp(std::abs(speed) / maxSpeed, 0.25f, 1.0f);
            float effectiveSteerRate = steerRate;
            if (drift.isDrifting) effectiveSteerRate *= driftSteerMultiplier;
            float steerAmount = steerInput * effectiveSteerRate * steerScale * deltaTime;
            if (std::abs(steerAmount) > 0.0f) {
                // Steer around world Y so steering only changes heading (yaw),
                // not pitch/roll — the fit-plane alignment handles tilt separately.
                carForward = glm::normalize(glm::rotate(glm::angleAxis(steerAmount, glm::vec3(0,1,0)), carForward));

                // Cornering friction: turning scrubs speed due to tire lateral force
                // More turning = more speed loss; reduced during drift (tires already sliding)
                float corneringFriction = 4.5f;  // speed loss per radian of steering
                float driftCorneringScale = 0.35f; // how much cornering friction applies during drift
                float frictionScale = drift.isDrifting ? driftCorneringScale : 1.0f;
                float speedLoss = std::abs(steerAmount) * corneringFriction * frictionScale * std::abs(speed);
                speed -= glm::sign(speed) * glm::min(speedLoss, std::abs(speed) * 0.5f * deltaTime);
            }

            // Acceleration/Braking
            if (input.forward) speed += accel * deltaTime;
            if (input.backward) speed -= reverseAccel * deltaTime;
            // During drift, brake doesn't slow you down as much (you're sliding)
            if (input.brake && !drift.isDrifting) speed *= glm::max(0.0f, 1.0f - brakeDamping * deltaTime);
            else if (input.brake && drift.isDrifting) speed *= glm::max(0.0f, 1.0f - brakeDamping * 0.15f * deltaTime);
            speed *= glm::max(0.0f, 1.0f - coastDamping * deltaTime);
            speed = glm::clamp(speed, -maxReverseSpeed, maxSpeed);

            // Initialize velocity direction if not set
            if (glm::length(drift.velocityDir) < 0.5f) drift.velocityDir = carForward;

            // Update velocity direction based on drift state
            if (drift.isDrifting) {
                // During drift: velocity direction lags behind facing direction
                // Apply lateral friction to slowly pull velocity toward facing
                float realignRate = driftFriction * deltaTime;
                drift.velocityDir = glm::normalize(glm::mix(drift.velocityDir, carForward, realignRate));

                // Compute current drift angle
                float dot = glm::clamp(glm::dot(carForward, drift.velocityDir), -1.0f, 1.0f);
                float cross = carForward.x * drift.velocityDir.z - carForward.z * drift.velocityDir.x;
                drift.driftAngle = std::atan2(cross, dot);

                // Clamp max drift angle
                if (std::abs(drift.driftAngle) > maxDriftAngle) {
                    float clampedAngle = glm::sign(drift.driftAngle) * maxDriftAngle;
                    // Rotate carForward to be maxDriftAngle away from velocityDir
                    drift.velocityDir = glm::normalize(glm::rotate(
                        glm::angleAxis(-clampedAngle, glm::vec3(0,1,0)), carForward));
                    drift.driftAngle = clampedAngle;
                }
            } else {
                // Not drifting: quickly realign velocity to facing direction
                float realignRate = glm::clamp(driftRecoveryRate * deltaTime, 0.0f, 1.0f);
                drift.velocityDir = glm::normalize(glm::mix(drift.velocityDir, carForward, realignRate));
                float dot = glm::clamp(glm::dot(carForward, drift.velocityDir), -1.0f, 1.0f);
                float cross = carForward.x * drift.velocityDir.z - carForward.z * drift.velocityDir.x;
                drift.driftAngle = std::atan2(cross, dot);
            }

            // Move car along velocity direction (not facing direction)
            glm::vec3 candidatePos = carPos + drift.velocityDir * speed * deltaTime;

            // --- GROUND ALIGNMENT (fit-plane) ---
            std::vector<glm::vec3> hitPoints;
            std::vector<glm::vec3> wheelWorlds;
            std::vector<bool> hasHits;
            for (const auto& wheel : vehicle.wheels) {
                glm::vec3 worldWheel = candidatePos + rotMat * wheel.connectionPoint;
                wheelWorlds.push_back(worldWheel);
                glm::vec3 rayStart = glm::vec3(worldWheel.x, candidatePos.y + raycastHeight, worldWheel.z);
                glm::vec3 down = glm::vec3(0, -1, 0);
                RaycastHit hit = raycastTrack(rayStart, down, 10.0f, trackCollider);
                if (hit.hit) {
                    glm::vec3 hitPt = rayStart + down * hit.distance;
                    hitPoints.push_back(hitPt);
                    hasHits.push_back(true);
                } else {
                    hitPoints.push_back(worldWheel);
                    hasHits.push_back(false);
                }
            }
            // Count valid hits
            int validHits = 0;
            for (size_t i = 0; i < hasHits.size(); ++i) { if (hasHits[i]) validHits++; }
            bool isGrounded = (validHits >= 3);
            if (!isGrounded) {
                // Airborne: apply gravity with accumulated vertical velocity
                // speed *= glm::max(0.0f, 1.0f - coastDamping * deltaTime);
                // m_fallSpeed[id] += 9.81f * deltaTime; // accumulate gravity
                // candidatePos.y -= m_fallSpeed[id] * deltaTime;
                // rb->m_position = candidatePos;
                // rb->m_velocity = carForward * speed;
                // m_forwardSpeed[id] = speed;
                // rb->updateTransform();
                continue;
            }
            m_fallSpeed[id] = 0.0f; // reset fall speed when grounded
            // Compute plane normal using two spanning vectors
            // Wheels: 0=FL, 1=FR, 2=RL, 3=RR
            // across = left-to-right, along = front-to-rear
            glm::vec3 across(0.0f), along(0.0f);
            bool hasAcross = false, hasAlong = false;
            if (hasHits[0] && hasHits[1]) { across = hitPoints[1] - hitPoints[0]; hasAcross = true; }
            else if (hasHits[2] && hasHits[3]) { across = hitPoints[3] - hitPoints[2]; hasAcross = true; }
            if (hasHits[0] && hasHits[2]) { along = hitPoints[2] - hitPoints[0]; hasAlong = true; }
            else if (hasHits[1] && hasHits[3]) { along = hitPoints[3] - hitPoints[1]; hasAlong = true; }
            glm::vec3 normal(0, 1, 0);
            if (hasAcross && hasAlong) {
                glm::vec3 n = glm::cross(along, across);
                if (glm::length(n) > 0.0001f) {
                    normal = glm::normalize(n);
                    if (normal.y < 0.0f) normal = -normal; // ensure pointing up
                }
            }
            // Project car's forward vector onto plane for new forward
            glm::vec3 forwardOnPlane = carForward - normal * glm::dot(carForward, normal);
            if (glm::length(forwardOnPlane) > 0.001f) forwardOnPlane = glm::normalize(forwardOnPlane); else forwardOnPlane = glm::vec3(0,0,1);
            glm::vec3 right = glm::cross(normal, forwardOnPlane);
            if (glm::length(right) < 0.001f) right = glm::vec3(1,0,0); else right = glm::normalize(right);
            glm::mat3 targetRotation(right, normal, forwardOnPlane);
            glm::quat targetOrientation = glm::normalize(glm::quat_cast(targetRotation));
            // Compute target Y: for each valid wheel hit, figure out where the car body
            // needs to be so that wheel rests on the ground
            float targetY = 0.0f;
            int hitCount = 0;
            for (size_t i = 0; i < hitPoints.size(); ++i) {
                if (!hasHits[i]) continue;
                // The wheel's local Y offset from the car body
                float wheelLocalY = (rotMat * vehicle.wheels[i].connectionPoint).y;
                // Car body Y should be: hitPoint.y + wheelRadius - wheelLocalY
                targetY += hitPoints[i].y + wheelRadius - wheelLocalY;
                hitCount++;
            }
            if (hitCount > 0) targetY /= (float)hitCount;
            else targetY = candidatePos.y;

            // --- WALL COLLISION ---
            auto wallProbe = [&](const glm::vec3& origin, const glm::vec3& dir, float dist, float clearance) {
                RaycastHit hit = raycastTrack(origin, dir, dist, trackCollider);
                if (!hit.hit) return;
                glm::vec3 wallN = glm::normalize(hit.normal);
                if (glm::dot(wallN, dir) > 0.0f) wallN = -wallN;
                // Only react to near-vertical surfaces (walls)
                if (std::abs(wallN.y) > 0.3f) return;
                float pen = clearance - hit.distance;
                if (pen <= 0.0f) return;
                candidatePos += wallN * (pen + wallPushback * 0.25f);
                float approach = glm::dot(forwardOnPlane, wallN);
                if (approach < 0.0f) {
                    speed *= glm::clamp(1.0f - (-approach * wallBounce), 0.0f, 1.0f);
                }
            };
            glm::vec3 probeUp = normal * 0.25f;
            float dynWallDist = wallProbeDist + std::abs(speed) * deltaTime;
            // Front probes
            wallProbe(candidatePos + probeUp, forwardOnPlane, dynWallDist, wallClearanceForward);
            wallProbe(candidatePos + right * wallProbeHalfWidth + probeUp, forwardOnPlane, dynWallDist, wallClearanceForward);
            wallProbe(candidatePos - right * wallProbeHalfWidth + probeUp, forwardOnPlane, dynWallDist, wallClearanceForward);
            // Rear probes
            wallProbe(candidatePos + probeUp, -forwardOnPlane, dynWallDist, wallClearanceForward);
            // Side probes
            wallProbe(candidatePos + probeUp, right, wallClearanceSide + 0.25f, wallClearanceSide);
            wallProbe(candidatePos + probeUp, -right, wallClearanceSide + 0.25f, wallClearanceSide);

            // Smoothly interpolate orientation; set X/Z directly, smooth only Y
            float rotAlpha = glm::clamp(orientationSmooth * deltaTime, 0.0f, 1.0f);
            float posAlpha = glm::clamp(heightSmooth * deltaTime, 0.0f, 1.0f);
            rb->m_orientation = glm::normalize(glm::slerp(rb->m_orientation, targetOrientation, rotAlpha));
            rb->m_position.x = candidatePos.x;
            rb->m_position.z = candidatePos.z;
            rb->m_position.y = glm::mix(rb->m_position.y, targetY, posAlpha);
            rb->m_velocity = forwardOnPlane * speed;
            rb->m_angularVelocity = glm::vec3(0.0f);
            rb->updateTransform();
            m_forwardSpeed[id] = speed;
        }

        for (auto const& [id, body] : bodyMap) {
            objects[id].modelTransform.pos = body->m_position - glm::vec3(0.0f, body->m_obb.halfExtents.y, 0.0f);
            objects[id].modelTransform.rot = glm::eulerAngles(body->m_orientation);
        }

        physicsEngine.step(deltaTime);
    }

    void OnPreRender(Shader& shader, float deltaTime) override {}

    void OnPostRender(Shader& shader, float deltaTime) override {
        for (auto& el : objects) {
            shader.setBool("debug", false);
            el.second.model->draw(shader, el.second.modelTransform);
        }

        if (m_wireframe) {
            // --- Debug draw for wheels, raycasts, hit points, and fit plane ---
            const auto& car = vehicles["Player"];
            glm::vec3 carPos = car.body->m_position;
            glm::mat3 carRot = car.body->m_obb.rotation;
            float wheelRadius = 0.3f;
            float raycastHeight = 2.0f;
            std::vector<glm::vec3> wheelWorlds;
            std::vector<glm::vec3> hitPoints;
            std::vector<bool> hasHits;
            std::vector<glm::vec3> rayStarts;
            std::vector<glm::vec3> rayEnds;
            // For each wheel
            for (const auto& wheel : car.wheels) {
                glm::vec3 worldWheel = carPos + carRot * wheel.connectionPoint;
                wheelWorlds.push_back(worldWheel);
                glm::vec3 rayStart = glm::vec3(worldWheel.x, carPos.y + raycastHeight, worldWheel.z);
                glm::vec3 down = glm::vec3(0, -1, 0);
                RaycastHit hit = raycastTrack(rayStart, down, 10.0f, trackCollider);
                rayStarts.push_back(rayStart);
                if (hit.hit) {
                    glm::vec3 hitPt = rayStart + down * hit.distance;
                    hitPoints.push_back(hitPt);
                    hasHits.push_back(true);
                    rayEnds.push_back(hitPt);
                } else {
                    hitPoints.push_back(worldWheel);
                    hasHits.push_back(false);
                    rayEnds.push_back(rayStart + down * 10.0f);
                }
            }
            // Draw wheel centers
            for (const auto& pos : wheelWorlds) {
                Transform debugTransform{pos, glm::vec3(0.0f), glm::vec3(0.1f)};
                shader.setVec3("material.baseColor", glm::vec3(0.0f, 1.0f, 0.0f));
                shader.setBool("debug", true);
                debugCube->draw(shader, debugTransform);
            }
            // Draw wheel radius as vertical cylinders (approximate with cubes for now)
            for (const auto& pos : wheelWorlds) {
                for (int i = -5; i <= 5; ++i) {
                    float t = (float)i / 5.0f;
                    glm::vec3 p = pos + glm::vec3(0, -t * wheelRadius, 0);
                    Transform debugTransform{p, glm::vec3(0.0f), glm::vec3(0.05f)};
                    shader.setVec3("material.baseColor", glm::vec3(0.2f, 0.8f, 0.2f));
                    shader.setBool("debug", true);
                    debugCube->draw(shader, debugTransform);
                }
            }
            // Draw raycasts
            for (size_t i = 0; i < rayStarts.size(); ++i) {
                glm::vec3 start = rayStarts[i];
                glm::vec3 end = rayEnds[i];
                int steps = 10;
                for (int s = 0; s <= steps; ++s) {
                    float t = (float)s / steps;
                    glm::vec3 p = glm::mix(start, end, t);
                    Transform debugTransform{p, glm::vec3(0.0f), glm::vec3(0.03f)};
                    shader.setVec3("material.baseColor", glm::vec3(0.8f, 0.8f, 0.2f));
                    shader.setBool("debug", true);
                    debugCube->draw(shader, debugTransform);
                }
            }
            // Draw hit points
            for (size_t i = 0; i < hitPoints.size(); ++i) {
                if (hasHits[i]) {
                    Transform debugTransform{hitPoints[i], glm::vec3(0.0f), glm::vec3(0.08f)};
                    shader.setVec3("material.baseColor", glm::vec3(1.0f, 0.0f, 0.0f));
                    shader.setBool("debug", true);
                    debugCube->draw(shader, debugTransform);
                }
            }
            // Draw fit plane if at least 3 hits
            std::vector<glm::vec3> planePoints;
            for (size_t i = 0; i < hitPoints.size(); ++i) {
                if (hasHits[i]) {
                    planePoints.push_back(hitPoints[i]);
                }
            }
            if (planePoints.size() >= 3) {
                // Fit plane using spanning vectors
                // Wheels: 0=FL, 1=FR, 2=RL, 3=RR
                glm::vec3 centroid(0.0f);
                for (const auto& p : planePoints) centroid += p;
                centroid /= (float)planePoints.size();
                glm::vec3 across(0.0f), along(0.0f);
                bool hasAcrossDbg = false, hasAlongDbg = false;
                if (hasHits[0] && hasHits[1]) { across = hitPoints[1] - hitPoints[0]; hasAcrossDbg = true; }
                else if (hasHits[2] && hasHits[3]) { across = hitPoints[3] - hitPoints[2]; hasAcrossDbg = true; }
                if (hasHits[0] && hasHits[2]) { along = hitPoints[2] - hitPoints[0]; hasAlongDbg = true; }
                else if (hasHits[1] && hasHits[3]) { along = hitPoints[3] - hitPoints[1]; hasAlongDbg = true; }
                glm::vec3 normal(0, 1, 0);
                if (hasAcrossDbg && hasAlongDbg) {
                    glm::vec3 n = glm::cross(along, across);
                    if (glm::length(n) > 0.0001f) {
                        normal = glm::normalize(n);
                        if (normal.y < 0.0f) normal = -normal;
                    }
                }
                // Draw a quad for the plane
                glm::vec3 ref = (fabs(normal.y) < 0.99f) ? glm::vec3(0,1,0) : glm::vec3(1,0,0);
                glm::vec3 u = glm::normalize(glm::cross(ref, normal));
                glm::vec3 v = glm::normalize(glm::cross(normal, u));
                float planeSize = 0.8f;
                for (int i = -2; i <= 2; ++i) {
                    for (int j = -2; j <= 2; ++j) {
                        glm::vec3 p = centroid + u * planeSize * (float)i + v * planeSize * (float)j;
                        Transform debugTransform{p, glm::vec3(0.0f), glm::vec3(0.04f)};
                        shader.setVec3("material.baseColor", glm::vec3(0.2f, 0.2f, 1.0f));
                        shader.setBool("debug", true);
                        debugCube->draw(shader, debugTransform);
                    }
                }
            }
            // Draw car rigid body
            Transform carRidgidBodyTransform{car.body->m_obb.center, glm::eulerAngles(car.body->m_orientation), car.body->m_obb.halfExtents};
            shader.setVec3("material.baseColor", glm::vec3(0.0f, 0.0f, 1.0f));
            shader.setBool("debug", true);
            debugCube->draw(shader, carRidgidBodyTransform);
        }

        if (m_debug) {
            for (glm::vec3 point : m_sceneManager->getScene().trackSpline._points) {
                shader.setVec3("material.baseColor", glm::vec3(1.0f, 0.0f, 0.0f));
                Transform debugTransform{point, glm::vec3(0.0f), glm::vec3(0.1f)};
                shader.setBool("debug", false);
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
    std::unordered_map<std::string, float> m_fallSpeed;
    std::unordered_map<std::string, GroundState> m_groundStates;

    struct DriftState {
        glm::vec3 velocityDir = glm::vec3(0, 0, 1); // direction the car is actually moving
        bool isDrifting = false;
        float driftAngle = 0.0f; // signed angle between facing and velocity
    };
    std::unordered_map<std::string, DriftState> m_driftStates;

    ModelResource* trackModel = nullptr;
    TrackCollider trackCollider;
};