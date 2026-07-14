#ifndef MESHSLICE_SLICER_H
#define MESHSLICE_SLICER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "mesh/mesh.h"

namespace meshslice {

// A single planar cross-section polyline in the slicing plane (z = Layer::z).
// Points are stored in stitch order; for a closed contour the last point wraps
// to the first (the first point is not repeated at the end).
struct Contour {
    std::vector<Vec3> points;
    bool closed = false;
    // Sum of segment lengths, including the wrap segment when closed.
    double perimeter = 0.0;
    // Shoelace area in the XY plane; positive for counter-clockwise winding.
    // Zero for open contours (area of an open polyline is not meaningful).
    double signed_area = 0.0;
};

struct Layer {
    float z = 0.0f;
    std::vector<Contour> contours;
};

struct SliceParams {
    float layer_height = 1.0f;
    size_t threads = 0;  // 0 -> hardware_concurrency.
    // Vertices closer than this to the slicing plane are treated as lying an
    // epsilon above it, so a plane through a vertex still yields closed loops.
    float on_plane_epsilon = 1e-5f;
    // Segment endpoints closer than this are considered coincident when
    // stitching segments into contours.
    float stitch_epsilon = 1e-4f;
};

struct SliceResult {
    std::vector<Layer> layers;
    // Parallel backend actually used at runtime: "openmp" or "thread_pool".
    std::string backend;
    size_t threads_used = 0;
};

// Slice the whole mesh into uniform layers. Planes are placed at the midpoint
// of each layer band: z = min + (i + 0.5) * layer_height, covering [min, max].
SliceResult slice_mesh(const Mesh& mesh, const SliceParams& params);

// Cross-section of the mesh with the single plane z = plane_z. Contours are
// returned in a deterministic order independent of thread count.
std::vector<Contour> slice_plane(const Mesh& mesh, float plane_z, const SliceParams& params);

// Slice at an explicit ascending list of plane heights. Both uniform and
// adaptive slicing funnel through this; layers run in parallel and land in
// per-index slots, so output is independent of scheduling.
SliceResult slice_at_planes(const Mesh& mesh, const std::vector<float>& planes,
                            const SliceParams& params);

// Number of layers slice_mesh will produce for a mesh spanning [z_min, z_max].
size_t layer_count_for_range(float z_min, float z_max, float layer_height);

}  // namespace meshslice

#endif  // MESHSLICE_SLICER_H
