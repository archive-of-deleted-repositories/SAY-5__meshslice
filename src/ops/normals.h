#ifndef MESHSLICE_OPS_NORMALS_H
#define MESHSLICE_OPS_NORMALS_H

#include <cstdint>
#include <map>

#include "mesh/mesh.h"
#include "partition/work_unit.h"

namespace meshslice {

// Per-vertex area-weighted normal contributions from one work unit. Keyed by
// global vertex index so the merger can accumulate contributions from every
// partition that touches a vertex, then normalize once at the end. Using an
// ordered map keeps the merge canonical and floating-point-deterministic.
struct NormalResult {
    std::map<uint32_t, Vec3> accum;  // vertex index -> summed (unnormalized) normal.
};

NormalResult normals_unit(const Mesh& mesh, const WorkUnit& unit);

}  // namespace meshslice

#endif  // MESHSLICE_OPS_NORMALS_H
