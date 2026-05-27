#ifndef MESHSLICE_MESH_H
#define MESHSLICE_MESH_H

#include <cstddef>
#include <vector>

#include "mesh/types.h"

namespace meshslice {

// A triangle mesh: vertices plus index triples. Loaders may produce duplicate
// vertices (e.g. raw STL); welding is intentionally left to operators that need it.
class Mesh {
   public:
    std::vector<Vec3> vertices;
    std::vector<Triangle> triangles;

    size_t triangle_count() const {
        return triangles.size();
    }
    size_t vertex_count() const {
        return vertices.size();
    }
    bool empty() const {
        return triangles.empty();
    }

    // Centroid of a triangle, used by the spatial partitioner.
    Vec3 centroid(size_t tri) const {
        const Triangle& t = triangles[tri];
        return (vertices[t.v[0]] + vertices[t.v[1]] + vertices[t.v[2]]) * (1.0f / 3.0f);
    }

    // Geometric vertices of a triangle.
    std::array<Vec3, 3> corners(size_t tri) const {
        const Triangle& t = triangles[tri];
        return {vertices[t.v[0]], vertices[t.v[1]], vertices[t.v[2]]};
    }

    Aabb bounds() const;
};

// Surface area of a single triangle from its three corner positions.
float triangle_area(const Vec3& a, const Vec3& b, const Vec3& c);

}  // namespace meshslice

#endif  // MESHSLICE_MESH_H
