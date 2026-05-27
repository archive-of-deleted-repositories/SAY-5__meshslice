#include "ops/voxel_count.h"

#include <algorithm>

namespace meshslice {

namespace {

uint32_t axis_voxel(float coord, float origin, float ext, uint32_t resolution) {
    if (ext <= 0.0f) {
        return 0;
    }
    long v = static_cast<long>((coord - origin) / ext * static_cast<float>(resolution));
    v = std::clamp<long>(v, 0, static_cast<long>(resolution) - 1);
    return static_cast<uint32_t>(v);
}

}  // namespace

VoxelResult voxel_count_unit(const Mesh& mesh, const WorkUnit& unit, const Aabb& global,
                             uint32_t resolution) {
    VoxelResult r;
    if (resolution == 0) {
        return r;
    }
    const Vec3 ext = global.extent();
    for (uint32_t t : unit.triangles) {
        // Mark the voxel containing each of the triangle's three corners. This is
        // a corner-sampling occupancy proxy: cheap, deterministic, and sufficient
        // for the parallel-merge story (a full conservative rasterization is out
        // of scope for the operator set).
        for (const Vec3& p : mesh.corners(t)) {
            const uint64_t vi = axis_voxel(p.x, global.min.x, ext.x, resolution);
            const uint64_t vj = axis_voxel(p.y, global.min.y, ext.y, resolution);
            const uint64_t vk = axis_voxel(p.z, global.min.z, ext.z, resolution);
            r.occupied.insert((vi * resolution + vj) * resolution + vk);
        }
    }
    return r;
}

}  // namespace meshslice
