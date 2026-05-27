#ifndef MESHSLICE_OPS_DECIMATE_H
#define MESHSLICE_OPS_DECIMATE_H

#include <cstdint>
#include <vector>

#include "mesh/mesh.h"
#include "partition/work_unit.h"

namespace meshslice {

// The decimated triangles for one work unit, expressed against the original
// global vertex array (so merging is a deterministic concatenation in partition
// order). Each surviving triangle keeps its global vertex indices.
struct DecimateResult {
    std::vector<Triangle> kept;
};

// Edge-collapse simplification toward `target_ratio` of the unit's triangles.
// Within a unit, the shortest edges are collapsed first (greedy), collapsing the
// second vertex onto the first and dropping any triangle that becomes degenerate.
// Operating per-unit keeps the work embarrassingly parallel; the boundary between
// units is preserved (no collapse crosses a partition), which is the documented
// correctness trade-off for parallelism.
DecimateResult decimate_unit(const Mesh& mesh, const WorkUnit& unit, float target_ratio);

}  // namespace meshslice

#endif  // MESHSLICE_OPS_DECIMATE_H
