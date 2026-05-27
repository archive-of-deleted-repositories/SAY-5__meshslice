#ifndef MESHSLICE_MESH_TYPES_H
#define MESHSLICE_MESH_TYPES_H

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace meshslice {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    Vec3(float ax, float ay, float az) : x(ax), y(ay), z(az) {
    }

    Vec3 operator+(const Vec3& o) const {
        return {x + o.x, y + o.y, z + o.z};
    }
    Vec3 operator-(const Vec3& o) const {
        return {x - o.x, y - o.y, z - o.z};
    }
    Vec3 operator*(float s) const {
        return {x * s, y * s, z * s};
    }

    bool operator==(const Vec3& o) const {
        return x == o.x && y == o.y && z == o.z;
    }
};

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float length(const Vec3& a) {
    return std::sqrt(dot(a, a));
}

inline Vec3 normalize(const Vec3& a) {
    const float len = length(a);
    if (len <= 0.0f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return a * (1.0f / len);
}

// A triangle stored as three vertex indices into the mesh vertex array.
struct Triangle {
    std::array<uint32_t, 3> v{{0, 0, 0}};
};

struct Aabb {
    Vec3 min{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
             std::numeric_limits<float>::max()};
    Vec3 max{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
             std::numeric_limits<float>::lowest()};

    void expand(const Vec3& p) {
        min.x = std::min(min.x, p.x);
        min.y = std::min(min.y, p.y);
        min.z = std::min(min.z, p.z);
        max.x = std::max(max.x, p.x);
        max.y = std::max(max.y, p.y);
        max.z = std::max(max.z, p.z);
    }

    Vec3 extent() const {
        return max - min;
    }

    bool valid() const {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }
};

}  // namespace meshslice

#endif  // MESHSLICE_MESH_TYPES_H
