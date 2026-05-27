#include "ops/normals.h"

namespace meshslice {

NormalResult normals_unit(const Mesh& mesh, const WorkUnit& unit) {
    NormalResult r;
    for (uint32_t t : unit.triangles) {
        const Triangle& tri = mesh.triangles[t];
        auto c = mesh.corners(t);
        // Unnormalized cross product has magnitude proportional to twice the
        // triangle area, so summing it directly yields an area-weighted normal.
        const Vec3 fn = cross(c[1] - c[0], c[2] - c[0]);
        for (int k = 0; k < 3; ++k) {
            Vec3& acc = r.accum[tri.v[k]];
            acc = acc + fn;
        }
    }
    return r;
}

}  // namespace meshslice
