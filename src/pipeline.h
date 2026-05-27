#ifndef MESHSLICE_PIPELINE_H
#define MESHSLICE_PIPELINE_H

#include <cstdint>
#include <string>
#include <vector>

#include "mesh/mesh.h"
#include "merge/merger.h"

namespace meshslice {

enum class Op { Decimate, Normals, BboxStats, VoxelCount };

bool parse_op(const std::string& name, Op& out);
const char* op_name(Op op);

struct PipelineConfig {
    Op op = Op::BboxStats;
    uint32_t divisions = 8;    // grid cells per axis.
    size_t threads = 0;        // 0 -> hardware_concurrency.
    size_t max_in_flight = 0;  // 0 -> threads.
    uint32_t voxel_resolution = 64;
    float decimate_ratio = 0.5f;
};

// The merged result of a pipeline run, plus a stable hash of the canonical-order
// output used to prove determinism across thread counts.
struct PipelineResult {
    Op op;
    uint64_t output_hash = 0;
    size_t partitions = 0;
    size_t peak_in_flight = 0;

    // Populated per op for human-readable / golden output.
    MergedBbox bbox;
    uint64_t voxel_count = 0;
    size_t decimated_triangles = 0;
    size_t normal_vertices = 0;
};

PipelineResult run_pipeline(const Mesh& mesh, const PipelineConfig& cfg);

// Render a result as deterministic text (used by the CLI and golden e2e tests).
std::string format_result(const PipelineResult& r);

}  // namespace meshslice

#endif  // MESHSLICE_PIPELINE_H
