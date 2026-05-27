#include <gtest/gtest.h>

#include <vector>

#include "mesh/generators.h"
#include "pipeline.h"

using namespace meshslice;

namespace {

uint64_t run_hash(const Mesh& m, Op op, size_t threads, size_t in_flight) {
    PipelineConfig cfg;
    cfg.op = op;
    cfg.divisions = 8;
    cfg.threads = threads;
    cfg.max_in_flight = in_flight;
    cfg.voxel_resolution = 32;
    cfg.decimate_ratio = 0.5f;
    return run_pipeline(m, cfg).output_hash;
}

}  // namespace

// The headline guarantee: output is bit-identical regardless of thread count or
// scheduling. We hash the canonical-order merged output and compare across
// thread counts {1,2,4,8} for every operator.
TEST(Determinism, BitIdenticalAcrossThreadCounts) {
    const Mesh m = make_blob(3, 12345);
    const Op ops[] = {Op::BboxStats, Op::VoxelCount, Op::Normals, Op::Decimate};
    const size_t thread_counts[] = {1, 2, 4, 8};

    for (Op op : ops) {
        const uint64_t baseline = run_hash(m, op, 1, 1);
        for (size_t tc : thread_counts) {
            const uint64_t h = run_hash(m, op, tc, tc);
            EXPECT_EQ(h, baseline) << "op=" << op_name(op) << " threads=" << tc;
        }
    }
}

// Determinism must also hold when the in-flight cap differs from the worker count
// (so completion order varies independently of thread count).
TEST(Determinism, IndependentOfInFlightCap) {
    const Mesh m = make_sphere(24, 32);
    const uint64_t baseline = run_hash(m, Op::Normals, 8, 8);
    EXPECT_EQ(run_hash(m, Op::Normals, 8, 1), baseline);
    EXPECT_EQ(run_hash(m, Op::Normals, 8, 3), baseline);
    EXPECT_EQ(run_hash(m, Op::Normals, 4, 2), baseline);
}

TEST(Determinism, RepeatedRunsStable) {
    const Mesh m = make_blob(2, 777);
    const uint64_t a = run_hash(m, Op::Decimate, 4, 4);
    const uint64_t b = run_hash(m, Op::Decimate, 4, 4);
    EXPECT_EQ(a, b);
}
