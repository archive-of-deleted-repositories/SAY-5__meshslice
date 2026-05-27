#ifndef MESHSLICE_OPS_VOXEL_COUNT_H
#define MESHSLICE_OPS_VOXEL_COUNT_H

#include <cstdint>
#include <set>

#include "mesh/mesh.h"
#include "partition/work_unit.h"

namespace meshslice {

// Occupied voxels for one work unit, keyed against a global grid so counts from
// different units can be merged without double-counting shared voxels. The grid
// has `resolution` cells per axis spanning the supplied global AABB.
struct VoxelResult {
    std::set<uint64_t> occupied;  // linear voxel ids; sorted for deterministic merge.
};

VoxelResult voxel_count_unit(const Mesh& mesh, const WorkUnit& unit, const Aabb& global,
                             uint32_t resolution);

}  // namespace meshslice

#endif  // MESHSLICE_OPS_VOXEL_COUNT_H
