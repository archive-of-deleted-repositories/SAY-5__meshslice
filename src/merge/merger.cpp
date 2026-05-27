#include "merge/merger.h"

#include <cstring>

namespace meshslice {

uint64_t hash_bytes(const void* data, size_t size, uint64_t seed) {
    // FNV-1a, 64-bit. Order-sensitive, so callers must feed canonical-order data.
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint64_t h = seed;
    for (size_t i = 0; i < size; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

MergedBbox merge_bbox(const std::vector<BboxStats>& results) {
    MergedBbox m;
    Vec3 weighted{0, 0, 0};
    for (const BboxStats& r : results) {  // canonical partition order.
        if (r.triangles == 0) {
            continue;
        }
        m.box.expand(r.box.min);
        m.box.expand(r.box.max);
        weighted = weighted + r.area_weighted_centroid * static_cast<float>(r.surface_area);
        m.surface_area += r.surface_area;
        m.triangles += r.triangles;
    }
    if (m.surface_area > 0.0) {
        m.area_weighted_centroid = weighted * static_cast<float>(1.0 / m.surface_area);
    }
    return m;
}

std::map<uint32_t, Vec3> merge_normals(const std::vector<NormalResult>& results) {
    std::map<uint32_t, Vec3> acc;
    for (const NormalResult& r : results) {  // canonical partition order.
        for (const auto& [vid, n] : r.accum) {
            acc[vid] = acc[vid] + n;
        }
    }
    std::map<uint32_t, Vec3> out;
    for (const auto& [vid, n] : acc) {
        out[vid] = normalize(n);
    }
    return out;
}

uint64_t merge_voxels(const std::vector<VoxelResult>& results, uint32_t /*resolution*/) {
    std::set<uint64_t> all;
    for (const VoxelResult& r : results) {
        all.insert(r.occupied.begin(), r.occupied.end());
    }
    return static_cast<uint64_t>(all.size());
}

std::vector<Triangle> merge_decimate(const std::vector<DecimateResult>& results) {
    std::vector<Triangle> out;
    for (const DecimateResult& r : results) {  // canonical partition order = stable.
        out.insert(out.end(), r.kept.begin(), r.kept.end());
    }
    return out;
}

}  // namespace meshslice
