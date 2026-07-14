#ifndef MESHSLICE_ADAPTIVE_H
#define MESHSLICE_ADAPTIVE_H

#include <vector>

#include "mesh/mesh.h"
#include "slice/slicer.h"

namespace meshslice {

// Adaptive layer planning: use thin layers where the surface is shallow
// (normals nearly vertical, where stair-stepping is most visible) and thick
// layers on steep walls. Heights are bounded to [min_layer_height,
// max_layer_height].
struct AdaptiveParams {
    float min_layer_height = 0.05f;
    float max_layer_height = 0.5f;
    // Resolution of the per-band steepness profile sampled over the Z range.
    size_t profile_buckets = 256;
};

// Plane placement only: returns the ascending list of slicing plane Z values
// chosen for the mesh. Deterministic for a fixed mesh and parameters.
std::vector<float> plan_adaptive_planes(const Mesh& mesh, const AdaptiveParams& params);

// Slice at adaptively planned planes. Contour extraction runs in parallel with
// the same backend as slice_mesh; SliceParams::layer_height is ignored.
SliceResult slice_mesh_adaptive(const Mesh& mesh, const SliceParams& params,
                                const AdaptiveParams& adaptive);

}  // namespace meshslice

#endif  // MESHSLICE_ADAPTIVE_H
