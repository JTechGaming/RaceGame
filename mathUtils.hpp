#pragma once

#include <glm/glm.hpp>

class Spline {
public:
    Spline(std::vector<glm::vec3> points) {
        _points = points;
    }

    void addPoint(glm::vec3 pos) {
        _points.emplace_back(pos);
    }

    int length() {
        return _points.size();
    }

    std::vector<glm::vec3> _points;
};

class MathUtils {
    static glm::vec3 distance(glm::vec3 point1, glm::vec3 point2);
    static glm::vec3 lerp(const glm::vec3& start, const glm::vec3& end, float t);
    static float getProjectionParameter(const glm::vec3& point, const glm::vec3& lineStart, const glm::vec3& lineEnd);
};
