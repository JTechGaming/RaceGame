#include "mathUtils.hpp"

glm::vec3 MathUtils::distance(glm::vec3 point1, glm::vec3 point2) {
    return point2 - point1;
}

glm::vec3 MathUtils::lerp(const glm::vec3& start, const glm::vec3& end, float t) {
    return start + t * (end - start);
}

float MathUtils::getProjectionParameter(const glm::vec3& point, const glm::vec3& lineStart, const glm::vec3& lineEnd) {
    glm::vec3 lineDir = lineEnd - lineStart;
    float lineLengthSquared = glm::dot(lineDir, lineDir);

    if (lineLengthSquared == 0) {
        return 0.0f;
    }

    float t = glm::dot(point - lineStart, lineDir) / lineLengthSquared;
    return t;
}