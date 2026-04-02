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

struct Triangle {
    glm::vec3 v0;
    glm::vec3 v1;
    glm::vec3 v2;
    glm::vec3 normal;
};

struct TrackCollider {
    std::vector<Triangle> triangles;

    float cellSize = 10.0f;

    glm::vec3 minBound;
    glm::vec3 maxBound;

    int gridWidth;
    int gridDepth;

    std::vector<std::vector<int>> grid; // triangle indices
};

glm::vec3 applyTransform(const glm::vec3& v, const Transform& t) {
    glm::vec3 scaled = v * t.scale;

    glm::mat4 rot =
        glm::rotate(glm::mat4(1.0f), glm::radians(t.rot.x), glm::vec3(1,0,0)) *
        glm::rotate(glm::mat4(1.0f), glm::radians(t.rot.y), glm::vec3(0,1,0)) *
        glm::rotate(glm::mat4(1.0f), glm::radians(t.rot.z), glm::vec3(0,0,1));

    glm::vec4 rotated = rot * glm::vec4(scaled, 1.0f);

    return glm::vec3(rotated) + t.pos;
}

class MathUtils {
    static glm::vec3 distance(glm::vec3 point1, glm::vec3 point2);
    static glm::vec3 lerp(const glm::vec3& start, const glm::vec3& end, float t);
    static float getProjectionParameter(const glm::vec3& point, const glm::vec3& lineStart, const glm::vec3& lineEnd);
};
