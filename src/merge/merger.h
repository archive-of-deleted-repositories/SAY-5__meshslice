#ifndef MESHSLICE_MERGER_H
#define MESHSLICE_MERGER_H

#include <cstdint>
#include <map>
#include <vector>

#include "mesh/mesh.h"
#include "ops/bbox_stats.h"
#include "ops/decimate.h"
#include "ops/normals.h"
#include "ops/voxel_count.h"

namespace meshslice {

// All mergers consume per-unit results ordered by canonical partition index
// (results[i] is the output of the unit with index i). The merge result is
// therefore independent of completion order or thread count: bit-identical.

struct MergedBbox {
    Aabb box;
    Vec3 area_weighted_centroid{0, 0, 0};
    double surface_area = 0.0;
    uint64_t triangles = 0;
};

MergedBbox merge_bbox(const std::vector<BboxStats>& results);

// Returns one normalized normal per vertex index (sorted ascending), summing the
// area-weighted contributions across all partitions then normalizing once.
std::map<uint32_t, Vec3> merge_normals(const std::vector<NormalResult>& results);

uint64_t merge_voxels(const std::vector<VoxelResult>& results, uint32_t resolution);

std::vector<Triangle> merge_decimate(const std::vector<DecimateResult>& results);

// A stable 64-bit hash of any merged output, used by the determinism tests to
// prove bit-identical results across thread counts.
uint64_t hash_bytes(const void* data, size_t size, uint64_t seed = 1469598103934665603ull);

}  // namespace meshslice

#endif  // MESHSLICE_MERGER_H
