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

glm::vec3 MathUtils::findModelDimensions(ModelResource* model) {
    if (model == nullptr || model->getMeshes()->empty()) {
        return glm::vec3(0.0f);
    }

    glm::vec3 minVertex = model->getMeshes()->at(0).vertices[0].position;
    glm::vec3 maxVertex = model->getMeshes()->at(0).vertices[0].position;

    for (const auto& mesh : *model->getMeshes()) {
        for (const auto& vertex : mesh.vertices) {
            minVertex = glm::min(minVertex, vertex.position);
            maxVertex = glm::max(maxVertex, vertex.position);
        }
    }

    return maxVertex - minVertex;
}