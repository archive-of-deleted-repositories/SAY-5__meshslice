#include "partition/grid.h"

#include <algorithm>
#include <map>
#include <stdexcept>

namespace meshslice {

GridPartitioner::GridPartitioner(uint32_t divisions) : divisions_(divisions) {
    if (divisions_ == 0) {
        throw std::invalid_argument("grid divisions must be >= 1");
    }
}

std::vector<WorkUnit> GridPartitioner::partition(const Mesh& mesh) const {
    std::vector<WorkUnit> units;
    if (mesh.empty()) {
        return units;
    }

    const Aabb box = mesh.bounds();
    const Vec3 ext = box.extent();

    // Per-axis cell size. Degenerate (zero-width) axes collapse to a single cell
    // on that axis so a flat mesh still partitions cleanly.
    auto cell_size = [&](float e) -> float {
        return (e > 0.0f) ? e / static_cast<float>(divisions_) : 0.0f;
    };
    const Vec3 cs(cell_size(ext.x), cell_size(ext.y), cell_size(ext.z));

    auto axis_cell = [&](float coord, float origin, float size) -> uint32_t {
        if (size <= 0.0f) {
            return 0;
        }
        long c = static_cast<long>((coord - origin) / size);
        if (c < 0) {
            c = 0;
        }
        if (c >= static_cast<long>(divisions_)) {
            c = static_cast<long>(divisions_) - 1;  // clamp the max boundary inward.
        }
        return static_cast<uint32_t>(c);
    };

    // Group triangle indices by cell key. std::map keeps a deterministic key order
    // so the produced work-unit indices are stable across runs.
    std::map<uint64_t, std::vector<uint32_t>> buckets;
    std::map<uint64_t, std::array<uint32_t, 3>> coords;

    for (size_t t = 0; t < mesh.triangle_count(); ++t) {
        const Vec3 c = mesh.centroid(t);
        const uint32_t ci = axis_cell(c.x, box.min.x, cs.x);
        const uint32_t cj = axis_cell(c.y, box.min.y, cs.y);
        const uint32_t ck = axis_cell(c.z, box.min.z, cs.z);
        const uint64_t key = (static_cast<uint64_t>(ci) * divisions_ + cj) * divisions_ + ck;
        buckets[key].push_back(static_cast<uint32_t>(t));
        coords[key] = {ci, cj, ck};
    }

    units.reserve(buckets.size());
    uint32_t next = 0;
    for (auto& [key, tris] : buckets) {
        WorkUnit u;
        u.index = next++;
        u.cell = coords[key];
        u.triangles = std::move(tris);
        units.push_back(std::move(u));
    }
    return units;
}

}  // namespace meshslice
