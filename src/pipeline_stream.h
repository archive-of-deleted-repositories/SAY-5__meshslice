#ifndef MESHSLICE_PIPELINE_STREAM_H
#define MESHSLICE_PIPELINE_STREAM_H

#include <cstdint>
#include <functional>

#include "mesh/stl_stream.h"
#include "mesh/types.h"
#include "pipeline.h"

namespace meshslice {

// A triangle source drives the streaming pipeline: each call must invoke `sink`
// once per triangle, in a stable order, and may be called more than once (the
// engine makes two passes: one for the AABB, one for partitioned aggregation).
// Backed by an STL file, this never materializes the whole mesh; backed by an
// on-the-fly generator, it never stores the mesh at all.
using TriangleSource = std::function<void(const std::function<void(const StreamTriangle&)>&)>;

// Out-of-core configuration. Only the foldable operators (bbox_stats,
// voxel_count) are supported in streaming mode, because their per-partition state
// is a fixed-size aggregate that folds incrementally. divisions and
// voxel_resolution mirror the in-core PipelineConfig so streaming and in-core
// results can be compared directly.
struct StreamConfig {
    Op op = Op::BboxStats;
    uint32_t divisions = 8;
    uint32_t voxel_resolution = 64;
};

// Result of a streaming run. Peak aggregate state is bounded by the number of
// occupied grid cells times the per-cell aggregate size, independent of the
// triangle count, so peak memory stays flat as the input grows.
struct StreamResult {
    Op op;
    uint64_t triangles = 0;
    size_t occupied_cells = 0;
    uint64_t output_hash = 0;

    MergedBbox bbox;
    uint64_t voxel_count = 0;
};

StreamResult run_streaming(const TriangleSource& source, const StreamConfig& cfg);

// Adapt an STL file into a TriangleSource (re-opens per pass).
TriangleSource stl_file_source(const std::string& path, size_t chunk_triangles = 1024);

}  // namespace meshslice

#endif  // MESHSLICE_PIPELINE_STREAM_H
