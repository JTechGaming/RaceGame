#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

struct Object {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 force;
    float mass;
};

class PhysicsScene {
private:
    std::vector<Object*> _objects;
    glm::vec3 _gravity = glm::vec3(0, -9.81f, 0);
public:
    void SpawnObject(Object* object) { }
    void DeleteObject(Object* object) { }

    void Tick(float dt) {
        for (Object* obj : _objects) {
            obj->force += obj->mass * _gravity;
            obj->velocity += obj->force / obj->mass * dt;
            obj->position += obj->velocity * dt;
            obj->force = glm::vec3(0, 0, 0);
        }
    }
};

struct CollisionPoints {
    glm::vec3 A;
    glm::vec3 B;
    glm::vec3 normal;
    glm::vec3 depth;
    glm::vec3 hasCollision;
};

struct Transform {
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
};

struct Collider {
    virtual glm::vec3 FindFurthestPoint(glm::vec3 direction) const = 0;
};

struct MeshCollider : Collider {
private:
    std::vector<glm::vec3> _vertices;
public:
    glm::vec3 FindFurthestPoint(glm::vec3 direction) const override {
        glm::vec3 maxPoint;
        float maxDistance = -FLT_MAX;

        for (glm::vec3 vertex : _vertices) {
            float distance = glm::dot(vertex, direction);
            if (distance > maxDistance) {
                maxDistance = distance;
                maxPoint = vertex;
            }
        }

        return maxPoint;
    }
};

glm::vec3 Support(const Collider& A, const Collider& B, glm::vec3 direction) {
    return A.FindFurthestPoint(direction) - B.FindFurthestPoint(-direction);
}

struct Simplex {
private:
    std::array<glm::vec3, 4> _points;
    int _size;
public:
    Simplex() : _size(0) {}

    Simplex& operator=(std::initializer_list<glm::vec3> list) {
        _size = 0;
        for (glm::vec3 point : list) {
            _points[_size++] = point;
        }
        return *this;
    }

    void push_front(glm::vec3 point) {
        _points = { point, _points[0], _points[1], _points[2] };
        _size = std::min(_size + 1, 4);
    }

    glm::vec3& operator[](int i) { return _points[i]; }
    size_t size() const { return _size; }

    auto begin() const { return _points.begin(); }
    auto end() const { return _points.end() - (4 - _size); }
};

bool SameDirection(const glm::vec3& direction, const glm::vec3& ao) {
    return glm::dot(direction, ao) > 0;
}

bool Line(Simplex& points, glm::vec3 direction) {
    glm::vec3 a = points[0];
    glm::vec3 b = points[1];

    glm::vec3 ab = b-a;
    glm::vec3 ao = -a;

    if (SameDirection(ab, ao)) {
        direction = glm::cross(glm::cross(ab, ao), ab);
    } else {
        points = { a };
        direction = ao;
    }
    
    return false;
}

bool Triangle(Simplex& points, glm::vec3& direction) {
    glm::vec3 a = points[0];
    glm::vec3 b = points[1];
    glm::vec3 c = points[2];

    glm::vec3 ab = b-a;
    glm::vec3 ac = c-a;
    glm::vec3 ao = -a;
    glm::vec3 abc = glm::cross(ab, ac);

    if (SameDirection(glm::cross(abc, ac), ao)) {
        if (SameDirection(ac, ao)) {
            points = { a, c };
            direction = glm::cross(glm::cross(ac, ao), ac);
        } else {
            return Line(points = { a, b }, direction);
        }
    } else {
        if (SameDirection(glm::cross(ab, abc), ao)) {
            return Line(points = { a, b }, direction);
        } else {
            if (SameDirection(abc, ao)) {
                direction = abc;
            } else {
                points = {a, c, b };
                direction = -abc;
            }
        }
    }

    return false;
}

bool Tetrahedron(Simplex& points, glm::vec3& direction) {
    glm::vec3 a = points[0];
    glm::vec3 b = points[1];
    glm::vec3 c = points[2];
    glm::vec3 d = points[3];

    glm::vec3 ab = b-a;
    glm::vec3 ac = c-a;
    glm::vec3 ad = d-a;
    glm::vec3 ao = -a;

    glm::vec3 abc = glm::cross(ab, ac);
    glm::vec3 acd = glm::cross(ac, ad);
    glm::vec3 adb = glm::cross(ad, ab);

    if (SameDirection(abc, ao)) {
        return Triangle(points = {a, b, c }, direction);
    }

    if (SameDirection(acd, ao)) {
        return Triangle(points = {a, c, d }, direction);
    }

    if (SameDirection(adb, ao)) {
        return Triangle(points = {a, d, b }, direction);
    }

    return true;
}

bool NextSimplex(Simplex& points, glm::vec3& direction) {
    switch(points.size()) {
        case 2: return Line(points, direction);
        case 3: return Triangle(points, direction);
        case 4: return Tetrahedron(points, direction);
    }
}

bool runGJKCheck(const Collider& A, const Collider& B) {
    glm::vec3 support = Support(A, B, glm::vec3(1, 0, 0));
    Simplex points;
    points.push_front(support);
    glm::vec3 direction = -support; // new dir towards origin
    
    while(true) {
        support = Support(A, B, direction);

        if (glm::dot(support, direction) <= 0) {
            return false;
        }

        points.push_front(support);

        if (NextSimplex(points, direction)) {
            return true;
        }
    }
}

std::pair<std::vector<glm::vec4>, size_t> GetFaceNormals(
    const std::vector<glm::vec3>& polytope, const std::vector<size_t>& faces
) {
    std::vector<glm::vec4> normals;
    size_t minTriangle = 0;
    float minDistance = FLT_MAX;

    for (size_t i = 0; i < faces.size(); i += 3) {
        glm::vec3 a = polytope[faces[i]];
        glm::vec3 b = polytope[faces[i+1]];
        glm::vec3 c = polytope[faces[i+2]];
        glm::vec3 normal = glm::normalize(glm::cross(b-a, c-a));
        float distance = glm::dot(normal, a);
        if (distance < 0) {
            normal *= -1;
            distance *= -1;
        }

        normals.emplace_back(normal, distance);

        if (distance < minDistance) {
            minTriangle = i / 3;
            minDistance = distance;
        }
    }

    return { normals, minTriangle };
}

void AddIfUniqueEdge(
    std::vector<std::pair<size_t, size_t>>& edges,
    const std::vector<size_t>& faces,
    size_t a, size_t b
) {
    auto reverse = std::find(
        edges.begin(),
        edges.end(),
        std::make_pair(faces[b], faces[a])
    );
    
    if (reverse != edges.end()) {
        edges.erase(reverse);
    } else {
        edges.emplace_back(faces[a], faces[b]);
    }
}

CollisionPoints EPA(const Simplex& simplex, Collider& A, Collider& B) {
    std::vector<glm::vec3> polytope(simplex.begin(), simplex.end());
    std::vector<size_t> faces = {
        0, 1, 2,
        0, 3, 1,
        0, 2, 3,
        1, 3, 2
    };

    auto [normals, minFace] = GetFaceNormals(polytope, faces);

    glm::vec3 minNormal;
    float minDistance = FLT_MAX;

    while (minDistance == FLT_MAX) {
        minNormal = normals[minFace].xyz();
        minDistance = normals[minFace].w;

        glm::vec3 support = Support(A, B, minNormal);
        float sDistance = glm::dot(minNormal, support);

        if (abs(sDistance - minDistance) > 0.001f) {
            minDistance = FLT_MAX;

            std::vector<std::pair<size_t, size_t>> uniqueEdges;
            for (size_t i = 0; i < normals.size(); i++) {
                if (SameDirection(normals[i], support)) {
                    size_t f = i*3;
                    AddIfUniqueEdge(uniqueEdges, faces, f, f+1);
                    AddIfUniqueEdge(uniqueEdges, faces, f+1, f+2);
                    AddIfUniqueEdge(uniqueEdges, faces, f+2, f);

                    faces[f+2] = faces.back();
                    faces.pop_back();
                    faces[f+1] = faces.back();
                    faces.pop_back();
                    faces[f] = faces.back();
                    faces.pop_back();

                    normals[i] = normals.back();
                    normals.pop_back();

                    i--;
                }
            }

            std::vector<size_t> newFaces;
            for (auto[edgeIndex1, edgeIndex2] : uniqueEdges) {
                newFaces.push_back(edgeIndex1);
                newFaces.push_back(edgeIndex2);
                newFaces.push_back(polytope.size());
            }

            polytope.push_back(support);

            auto[newNormals, newMinFace] = GetFaceNormals(polytope, newFaces);

            float oldMinDistance = FLT_MAX;
            for (size_t i = 0; i < normals.size(); i++) {
                if (normals[i].w < oldMinDistance) {
                    oldMinDistance = normals[i].w;
                    minFace = i;
                }
            }

            if (newNormals[newMinFace].w < oldMinDistance) {
                minFace = newMinFace + normals.size();
            }

            faces.insert(faces.end(), newFaces.begin(), newFaces.end());
            normals.insert(normals.end(), newNormals.begin(), newNormals.end());
        }
    }

    CollisionPoints points;
    points.normal = minNormal;
    points.penetrationDepth = minDistance + 0.001f;
    points.hasCollision = true;

    return points;
}