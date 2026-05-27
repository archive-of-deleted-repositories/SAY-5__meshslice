#ifndef MESHSLICE_GRID_H
#define MESHSLICE_GRID_H

#include <cstdint>
#include <vector>

#include "mesh/mesh.h"
#include "partition/work_unit.h"

namespace meshslice {

// Spatial-grid partitioner. Lays a regular grid over the mesh AABB and assigns
// each triangle to exactly one cell by its centroid. A triangle that spans
// multiple cells is owned solely by the cell containing its centroid; this keeps
// the partition a true disjoint cover (every triangle assigned exactly once, no
// overlap, complete coverage). Empty cells produce no work unit.
class GridPartitioner {
   public:
    // divisions is the number of cells per axis (>= 1). The grid is divisions^3
    // cells at most; only non-empty cells become work units.
    explicit GridPartitioner(uint32_t divisions);

    std::vector<WorkUnit> partition(const Mesh& mesh) const;

   private:
    uint32_t divisions_;
};

}  // namespace meshslice

#endif  // MESHSLICE_GRID_H
