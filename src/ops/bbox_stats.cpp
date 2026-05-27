#include "ops/bbox_stats.h"

namespace meshslice {

BboxStats bbox_stats_unit(const Mesh& mesh, const WorkUnit& unit) {
    BboxStats s;
    Vec3 weighted{0, 0, 0};
    for (uint32_t t : unit.triangles) {
        auto c = mesh.corners(t);
        s.box.expand(c[0]);
        s.box.expand(c[1]);
        s.box.expand(c[2]);
        const float area = triangle_area(c[0], c[1], c[2]);
        const Vec3 centroid = (c[0] + c[1] + c[2]) * (1.0f / 3.0f);
        weighted = weighted + centroid * area;
        s.surface_area += static_cast<double>(area);
        ++s.triangles;
    }
    if (s.surface_area > 0.0) {
        s.area_weighted_centroid = weighted * static_cast<float>(1.0 / s.surface_area);
    }
    return s;
}

}  // namespace meshslice
