#include <gtest/gtest.h>

#include <cmath>

#include "mesh/generators.h"
#include "slice/slicer.h"

using namespace meshslice;

namespace {

// Unit octahedron with outward winding: apexes at z = +-1, four equator
// vertices in the z = 0 plane. Used to slice exactly through vertices.
Mesh make_octahedron() {
    Mesh m;
    m.vertices = {Vec3(1, 0, 0),  Vec3(0, 1, 0), Vec3(-1, 0, 0),
                  Vec3(0, -1, 0), Vec3(0, 0, 1), Vec3(0, 0, -1)};
    m.triangles = {
        Triangle{{0, 1, 4}}, Triangle{{1, 2, 4}}, Triangle{{2, 3, 4}}, Triangle{{3, 0, 4}},
        Triangle{{1, 0, 5}}, Triangle{{2, 1, 5}}, Triangle{{3, 2, 5}}, Triangle{{0, 3, 5}},
    };
    return m;
}

}  // namespace

TEST(Slicer, CubeMidHeightSingleSquareContour) {
    const Mesh cube = make_cube(0.5f);  // side length 1, z in [-0.5, 0.5].
    const std::vector<Contour> contours = slice_plane(cube, 0.0f, SliceParams{});
    ASSERT_EQ(contours.size(), 1u);
    const Contour& c = contours[0];
    EXPECT_TRUE(c.closed);
    EXPECT_NEAR(c.perimeter, 4.0, 1e-4);
    EXPECT_NEAR(std::abs(c.signed_area), 1.0, 1e-4);
}

TEST(Slicer, SphereLayerPerimeterMatchesCircle) {
    const Mesh sphere = make_sphere(64, 64, 1.0f);
    const float z = 0.5f;
    const std::vector<Contour> contours = slice_plane(sphere, z, SliceParams{});
    ASSERT_EQ(contours.size(), 1u);
    const Contour& c = contours[0];
    EXPECT_TRUE(c.closed);
    const double r = std::sqrt(1.0 - static_cast<double>(z) * z);
    const double circle = 2.0 * 3.14159265358979323846 * r;
    // A 64-gon inscribed in the circle is slightly shorter than the circle.
    EXPECT_LT(c.perimeter, circle);
    EXPECT_NEAR(c.perimeter, circle, circle * 0.02);
    EXPECT_NEAR(std::abs(c.signed_area), 3.14159265358979323846 * r * r,
                3.14159265358979323846 * r * r * 0.03);
}

TEST(Slicer, PlaneThroughVerticesStillCloses) {
    const Mesh octa = make_octahedron();
    // z = 0 passes exactly through the four equator vertices.
    const std::vector<Contour> contours = slice_plane(octa, 0.0f, SliceParams{});
    ASSERT_EQ(contours.size(), 1u);
    const Contour& c = contours[0];
    EXPECT_TRUE(c.closed);
    EXPECT_NEAR(c.perimeter, 4.0 * std::sqrt(2.0), 1e-3);
    // Outward-wound mesh: the outer contour is counter-clockwise.
    EXPECT_GT(c.signed_area, 0.0);
    EXPECT_NEAR(c.signed_area, 2.0, 1e-3);
}

TEST(Slicer, CrackedMeshYieldsOpenContour) {
    Mesh cracked = make_cube(0.5f);
    // Remove one +x side triangle that crosses z = 0; the loop cannot close.
    cracked.triangles.erase(cracked.triangles.begin() + 6);
    const std::vector<Contour> contours = slice_plane(cracked, 0.0f, SliceParams{});
    ASSERT_GE(contours.size(), 1u);
    bool any_open = false;
    for (const Contour& c : contours) {
        if (!c.closed) {
            any_open = true;
        }
    }
    EXPECT_TRUE(any_open);
}

TEST(Slicer, SliceMeshUniformLayers) {
    const Mesh cube = make_cube(0.5f);
    SliceParams params;
    params.layer_height = 0.25f;
    const SliceResult r = slice_mesh(cube, params);
    ASSERT_EQ(r.layers.size(), 4u);
    EXPECT_TRUE(r.backend == "openmp" || r.backend == "thread_pool");
    EXPECT_GE(r.threads_used, 1u);
    for (const Layer& layer : r.layers) {
        ASSERT_EQ(layer.contours.size(), 1u);
        EXPECT_TRUE(layer.contours[0].closed);
        EXPECT_NEAR(layer.contours[0].perimeter, 4.0, 1e-4);
    }
}

TEST(Slicer, LayerCountForRange) {
    EXPECT_EQ(layer_count_for_range(0.0f, 1.0f, 0.25f), 4u);
    EXPECT_EQ(layer_count_for_range(0.0f, 1.0f, 2.0f), 1u);
    EXPECT_EQ(layer_count_for_range(0.0f, 0.0f, 0.25f), 1u);
}
