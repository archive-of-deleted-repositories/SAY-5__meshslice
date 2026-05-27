#ifndef MESHSLICE_WORK_UNIT_H
#define MESHSLICE_WORK_UNIT_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace meshslice {

// A spatial work unit: the set of triangle indices whose centroids fall in one
// grid cell. The index gives the canonical merge order (deterministic regardless
// of which thread processes the unit). cell holds the (i,j,k) grid coordinate for
// NUMA-locality decisions in later layers.
struct WorkUnit {
    uint32_t index = 0;
    std::array<uint32_t, 3> cell{{0, 0, 0}};
    std::vector<uint32_t> triangles;

    size_t triangle_count() const {
        return triangles.size();
    }

    // Estimated peak bytes this unit holds in flight: the triangle index list.
    size_t approx_bytes() const {
        return triangles.capacity() * sizeof(uint32_t);
    }
};

}  // namespace meshslice

#endif  // MESHSLICE_WORK_UNIT_H
