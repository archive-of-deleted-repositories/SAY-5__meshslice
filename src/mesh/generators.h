#ifndef MESHSLICE_GENERATORS_H
#define MESHSLICE_GENERATORS_H

#include <cstdint>

#include "mesh/mesh.h"

namespace meshslice {

// Axis-aligned unit cube centered at the origin (12 triangles).
Mesh make_cube(float half_extent = 1.0f);

// UV sphere with the given stacks/slices, radius 1.
Mesh make_sphere(uint32_t stacks, uint32_t slices, float radius = 1.0f);

// Deterministic pseudo-random "bunny-like" low-poly blob: a perturbed icosphere.
Mesh make_blob(uint32_t subdivisions, uint64_t seed);

// A large synthetic grid mesh with approximately `target_triangles` triangles,
// laid out so the spatial partitioner produces many balanced cells. Used by the
// scaling benchmark and the out-of-core tests.
Mesh make_grid_mesh(size_t target_triangles, uint64_t seed = 1);

}  // namespace meshslice

#endif  // MESHSLICE_GENERATORS_H
