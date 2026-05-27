#include "mesh/mesh.h"

namespace meshslice {

Aabb Mesh::bounds() const {
    Aabb box;
    for (const Vec3& v : vertices) {
        box.expand(v);
    }
    return box;
}

float triangle_area(const Vec3& a, const Vec3& b, const Vec3& c) {
    return 0.5f * length(cross(b - a, c - a));
}

}  // namespace meshslice
