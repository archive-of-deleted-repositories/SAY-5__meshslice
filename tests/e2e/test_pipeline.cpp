#include <gtest/gtest.h>

#include <string>

#include "mesh/stl_loader.h"
#include "pipeline.h"

using namespace meshslice;

namespace {

std::string example(const std::string& name) {
    return std::string(MESHSLICE_EXAMPLES_DIR) + "/" + name;
}

PipelineResult run_op(const Mesh& m, Op op) {
    PipelineConfig cfg;
    cfg.op = op;
    cfg.divisions = 8;
    cfg.threads = 4;
    cfg.max_in_flight = 4;
    cfg.voxel_resolution = 32;
    return run_pipeline(m, cfg);
}

}  // namespace

// Golden end-to-end checks on the three committed example meshes. The cube is the
// strongest golden because its geometry is exact.
TEST(Pipeline, CubeBboxGolden) {
    const Mesh m = load_binary_stl(example("cube.stl"));
    const PipelineResult r = run_op(m, Op::BboxStats);
    EXPECT_EQ(r.bbox.triangles, 12u);
    EXPECT_NEAR(r.bbox.surface_area, 24.0, 1e-3);
    EXPECT_NEAR(r.bbox.box.min.x, -1.0f, 1e-4);
    EXPECT_NEAR(r.bbox.box.max.x, 1.0f, 1e-4);
}

TEST(Pipeline, AllExamplesLoadAndRunEveryOp) {
    for (const char* name : {"cube.stl", "sphere.stl", "bunny_lowpoly.stl"}) {
        const Mesh m = load_binary_stl(example(name));
        ASSERT_GT(m.triangle_count(), 0u) << name;
        for (Op op : {Op::BboxStats, Op::VoxelCount, Op::Normals, Op::Decimate}) {
            const PipelineResult r = run_op(m, op);
            EXPECT_GT(r.partitions, 0u) << name << " " << op_name(op);
            EXPECT_NE(r.output_hash, 0u) << name << " " << op_name(op);
        }
    }
}

TEST(Pipeline, SphereVoxelCountPositive) {
    const Mesh m = load_binary_stl(example("sphere.stl"));
    const PipelineResult r = run_op(m, Op::VoxelCount);
    EXPECT_GT(r.voxel_count, 0u);
}

TEST(Pipeline, FormatResultIsDeterministic) {
    const Mesh m = load_binary_stl(example("cube.stl"));
    const PipelineResult r = run_op(m, Op::BboxStats);
    EXPECT_EQ(format_result(r), format_result(r));
}
