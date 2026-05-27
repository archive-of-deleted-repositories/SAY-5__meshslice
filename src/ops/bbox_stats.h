#ifndef MESHSLICE_OPS_BBOX_STATS_H
#define MESHSLICE_OPS_BBOX_STATS_H

#include "mesh/mesh.h"
#include "partition/work_unit.h"

namespace meshslice {

// Per-partition geometric summary.
struct BboxStats {
    Aabb box;
    Vec3 area_weighted_centroid{0, 0, 0};
    double surface_area = 0.0;
    uint64_t triangles = 0;
};

// Compute AABB, area-weighted centroid, and total surface area for one work unit.
BboxStats bbox_stats_unit(const Mesh& mesh, const WorkUnit& unit);

}  // namespace meshslice

#endif  // MESHSLICE_OPS_BBOX_STATS_H
