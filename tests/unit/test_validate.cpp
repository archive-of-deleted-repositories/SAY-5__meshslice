#include <gtest/gtest.h>

#include "mesh/generators.h"
#include "slice/slicer.h"
#include "slice/validate.h"

using namespace meshslice;

TEST(Validate, GeneratedCubeIsWatertight) {
    const ValidationReport v = validate_mesh(make_cube(0.5f));
    EXPECT_EQ(v.non_manifold_edges, 0u);
    EXPECT_EQ(v.boundary_edges, 0u);
    EXPECT_EQ(v.boundary_loops, 0u);
    EXPECT_EQ(v.degenerate_triangles, 0u);
    EXPECT_TRUE(v.watertight());
}

TEST(Validate, CrackedCubeReportsOneBoundaryLoop) {
    Mesh cracked = make_cube(0.5f);
    cracked.triangles.erase(cracked.triangles.begin() + 6);
    const ValidationReport v = validate_mesh(cracked);
    EXPECT_EQ(v.boundary_edges, 3u);
    EXPECT_EQ(v.boundary_loops, 1u);
    EXPECT_FALSE(v.watertight());
}

TEST(Validate, DegenerateTriangleCounted) {
    Mesh m;
    m.vertices = {Vec3(0, 0, 0), Vec3(1, 0, 0), Vec3(2, 0, 0)};
    m.triangles = {Triangle{{0, 1, 2}}};  // collinear: zero area.
    const ValidationReport v = validate_mesh(m);
    EXPECT_EQ(v.degenerate_triangles, 1u);
}

TEST(Validate, NonManifoldEdgeCounted) {
    // Three triangles fanning around one shared edge.
    Mesh m;
    m.vertices = {Vec3(0, 0, 0), Vec3(0, 0, 1), Vec3(1, 0, 0), Vec3(0, 1, 0), Vec3(-1, 0, 0)};
    m.triangles = {Triangle{{0, 1, 2}}, Triangle{{0, 1, 3}}, Triangle{{0, 1, 4}}};
    const ValidationReport v = validate_mesh(m);
    EXPECT_EQ(v.non_manifold_edges, 1u);
}

TEST(Validate, FixNormalsMakesSliceAreasPositive) {
    Mesh cube = make_cube(0.5f);
    fix_normals(cube);
    const ValidationReport after = validate_mesh(cube);
    EXPECT_EQ(after.inverted_normals, 0u);

    const std::vector<Contour> contours = slice_plane(cube, 0.0f, SliceParams{});
    ASSERT_EQ(contours.size(), 1u);
    EXPECT_TRUE(contours[0].closed);
    EXPECT_GT(contours[0].signed_area, 0.0);
    EXPECT_NEAR(contours[0].signed_area, 1.0, 1e-4);
}

TEST(Validate, FixNormalsIsIdempotent) {
    Mesh cube = make_cube(0.5f);
    fix_normals(cube);
    EXPECT_EQ(fix_normals(cube), 0u);
}
