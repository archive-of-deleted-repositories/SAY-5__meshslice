#include <gtest/gtest.h>

#include <cmath>

#include "mesh/generators.h"
#include "ops/bbox_stats.h"
#include "ops/decimate.h"
#include "ops/normals.h"
#include "ops/voxel_count.h"
#include "partition/grid.h"

using namespace meshslice;

namespace {

// Single-unit helper: put the whole mesh in one work unit for scalar reference
// checks against hand-computed values.
WorkUnit whole_mesh_unit(const Mesh& m) {
    WorkUnit u;
    u.index = 0;
    for (size_t t = 0; t < m.triangle_count(); ++t) {
        u.triangles.push_back(static_cast<uint32_t>(t));
    }
    return u;
}

}  // namespace

TEST(Ops, BboxStatsOnUnitCube) {
    const Mesh cube = make_cube(1.0f);  // 2x2x2 cube, surface area = 6 faces * 4 = 24.
    const WorkUnit u = whole_mesh_unit(cube);
    const BboxStats s = bbox_stats_unit(cube, u);
    EXPECT_EQ(s.triangles, 12u);
    EXPECT_NEAR(s.surface_area, 24.0, 1e-4);
    EXPECT_NEAR(s.box.min.x, -1.0f, 1e-5);
    EXPECT_NEAR(s.box.max.z, 1.0f, 1e-5);
    EXPECT_NEAR(s.area_weighted_centroid.x, 0.0f, 1e-4);
    EXPECT_NEAR(s.area_weighted_centroid.y, 0.0f, 1e-4);
    EXPECT_NEAR(s.area_weighted_centroid.z, 0.0f, 1e-4);
}

TEST(Ops, NormalsOnSingleTriangle) {
    Mesh m;
    m.vertices = {Vec3(0, 0, 0), Vec3(1, 0, 0), Vec3(0, 1, 0)};
    m.triangles = {Triangle{{0, 1, 2}}};
    const NormalResult r = normals_unit(m, whole_mesh_unit(m));
    ASSERT_EQ(r.accum.size(), 3u);
    // Face lies in z=0 plane wound CCW, so the normal points +z.
    const Vec3 n = normalize(r.accum.at(0));
    EXPECT_NEAR(n.x, 0.0f, 1e-5);
    EXPECT_NEAR(n.y, 0.0f, 1e-5);
    EXPECT_NEAR(n.z, 1.0f, 1e-5);
}

TEST(Ops, VoxelCountSingleCellMesh) {
    Mesh m;
    m.vertices = {Vec3(0, 0, 0), Vec3(0, 0, 0), Vec3(0, 0, 0)};
    m.triangles = {Triangle{{0, 1, 2}}};
    Aabb global;
    global.expand(Vec3(0, 0, 0));
    global.expand(Vec3(1, 1, 1));
    const VoxelResult r = voxel_count_unit(m, whole_mesh_unit(m), global, 8);
    EXPECT_EQ(r.occupied.size(), 1u);  // all three corners collapse to one voxel.
}

TEST(Ops, VoxelCountDistinctCorners) {
    Mesh m;
    m.vertices = {Vec3(0, 0, 0), Vec3(0.99f, 0, 0), Vec3(0, 0.99f, 0)};
    m.triangles = {Triangle{{0, 1, 2}}};
    Aabb global;
    global.expand(Vec3(0, 0, 0));
    global.expand(Vec3(1, 1, 1));
    const VoxelResult r = voxel_count_unit(m, whole_mesh_unit(m), global, 4);
    EXPECT_EQ(r.occupied.size(), 3u);  // three corners land in three voxels.
}

TEST(Ops, DecimateReducesToTargetRatio) {
    const Mesh sphere = make_sphere(20, 30);
    const WorkUnit u = whole_mesh_unit(sphere);
    const DecimateResult r = decimate_unit(sphere, u, 0.5f);
    EXPECT_LT(r.kept.size(), sphere.triangle_count());
    EXPECT_GT(r.kept.size(), 0u);
    // No degenerate triangles survive.
    for (const Triangle& t : r.kept) {
        EXPECT_NE(t.v[0], t.v[1]);
        EXPECT_NE(t.v[1], t.v[2]);
        EXPECT_NE(t.v[0], t.v[2]);
    }
}

TEST(Ops, DecimateRatioOneKeepsAll) {
    const Mesh cube = make_cube();
    const DecimateResult r = decimate_unit(cube, whole_mesh_unit(cube), 1.0f);
    EXPECT_EQ(r.kept.size(), cube.triangle_count());
}
