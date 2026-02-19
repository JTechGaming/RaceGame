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

CollisionPoints EPA() {
    
}