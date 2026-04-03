#include "mathUtils.hpp" // voor de helper functies
#include <game.hpp>
#include <testScene.yml>

void updateNPC(NPC& npc, float deltaTime) {
    glm::vec3 targetPos = splinePoints[npc.currentTargetIndex];

    // Vector naar target
    glm::vec3 toTarget = MathUtils::distance(npc.body->m_position, targetPos);
    float distanceToTarget = glm::length(toTarget);

    // Richting naar target
    glm::vec3 direction = glm::normalize(toTarget);

    // Bepaal de snelheid van de NPC (pas dit aan)
    float npcSpeed = 20.0f;

    // Interpoleer positie
    glm::vec3 newPos = MathUtils::lerp(npc.body->m_position, targetPos, (npcSpeed * deltaTime) / distanceToTarget);
    npc.body->m_position = newPos;

    // Controleer of de NPC het doelpunt heeft gepasseerd
    float projectionParam = MathUtils::getProjectionParameter(newPos, splinePoints[npc.currentTargetIndex], splinePoints[(npc.currentTargetIndex + 1) % splinePoints.size()]);

    if (projectionParam >= 1.0f) {
        // Ga naar het volgende splinepunt
        npc.currentTargetIndex = (npc.currentTargetIndex + 1) % splinePoints.size();
    }
}

for (NPC& npc : npcs) {
    updateNPC(npc, deltaTime);
}