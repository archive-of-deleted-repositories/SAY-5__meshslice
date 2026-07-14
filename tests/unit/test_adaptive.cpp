#include <gtest/gtest.h>

#include <cmath>
#include <string>

#include "mesh/generators.h"
#include "slice/adaptive.h"
#include "slice/contour_io.h"
#include "slice/slicer.h"

using namespace meshslice;

namespace {

size_t planes_in_band(const std::vector<float>& planes, float lo, float hi) {
    size_t n = 0;
    for (float z : planes) {
        if (z >= lo && z <= hi) {
            ++n;
        }
    }
    return n;
}

}  // namespace

TEST(Adaptive, SpherePolesGetMoreLayersThanEquator) {
    const Mesh sphere = make_sphere(64, 64, 1.0f);
    AdaptiveParams params;
    params.min_layer_height = 0.02f;
    params.max_layer_height = 0.2f;
    const std::vector<float> planes = plan_adaptive_planes(sphere, params);
    ASSERT_GT(planes.size(), 2u);
    // Bands of equal height: near the north pole the surface is almost flat
    // (normals vertical), around the equator it is a vertical wall.
    const size_t pole = planes_in_band(planes, 0.7f, 1.0f);
    const size_t equator = planes_in_band(planes, -0.15f, 0.15f);
    EXPECT_GT(pole, equator);
}

TEST(Adaptive, PlaneSpacingRespectsBounds) {
    const Mesh sphere = make_sphere(64, 64, 1.0f);
    AdaptiveParams params;
    params.min_layer_height = 0.02f;
    params.max_layer_height = 0.2f;
    const std::vector<float> planes = plan_adaptive_planes(sphere, params);
    ASSERT_GT(planes.size(), 1u);
    // Midpoint gaps average two adjacent layer heights, so they stay within
    // the same bounds; the final partial layer may fall short of min.
    for (size_t i = 0; i + 1 < planes.size(); ++i) {
        const float gap = planes[i + 1] - planes[i];
        EXPECT_GT(gap, 0.0f);
        EXPECT_LE(gap, params.max_layer_height + 1e-4f);
        if (i + 2 < planes.size()) {
            EXPECT_GE(gap, params.min_layer_height - 1e-4f);
        }
    }
}

TEST(Adaptive, AdaptiveSliceContoursStayClosed) {
    const Mesh sphere = make_sphere(32, 32, 1.0f);
    AdaptiveParams adaptive;
    adaptive.min_layer_height = 0.05f;
    adaptive.max_layer_height = 0.3f;
    const SliceResult r = slice_mesh_adaptive(sphere, SliceParams{}, adaptive);
    ASSERT_GT(r.layers.size(), 0u);
    for (const Layer& layer : r.layers) {
        for (const Contour& c : layer.contours) {
            EXPECT_TRUE(c.closed);
        }
    }
}

TEST(ContourIo, JsonRoundTripIsExact) {
    Mesh cube = make_cube(0.5f);
    SliceParams params;
    params.layer_height = 0.25f;
    const SliceResult r = slice_mesh(cube, params);
    const SliceResult back = contours_from_json(contours_to_json(r));

    ASSERT_EQ(back.layers.size(), r.layers.size());
    EXPECT_EQ(back.backend, r.backend);
    EXPECT_EQ(back.threads_used, r.threads_used);
    for (size_t i = 0; i < r.layers.size(); ++i) {
        EXPECT_EQ(back.layers[i].z, r.layers[i].z);
        ASSERT_EQ(back.layers[i].contours.size(), r.layers[i].contours.size());
        for (size_t c = 0; c < r.layers[i].contours.size(); ++c) {
            const Contour& a = r.layers[i].contours[c];
            const Contour& b = back.layers[i].contours[c];
            EXPECT_EQ(b.closed, a.closed);
            EXPECT_EQ(b.perimeter, a.perimeter);
            EXPECT_EQ(b.signed_area, a.signed_area);
            ASSERT_EQ(b.points.size(), a.points.size());
            for (size_t p = 0; p < a.points.size(); ++p) {
                EXPECT_EQ(b.points[p], a.points[p]);
            }
        }
    }
}

TEST(ContourIo, FileRoundTrip) {
    const Mesh sphere = make_sphere(16, 16, 1.0f);
    SliceParams params;
    params.layer_height = 0.5f;
    const SliceResult r = slice_mesh(sphere, params);
    const std::string path = testing::TempDir() + "meshslice_contours_roundtrip.json";
    write_contours(r, path);
    const SliceResult back = read_contours(path);
    ASSERT_EQ(back.layers.size(), r.layers.size());
    for (size_t i = 0; i < r.layers.size(); ++i) {
        EXPECT_EQ(back.layers[i].z, r.layers[i].z);
        EXPECT_EQ(back.layers[i].contours.size(), r.layers[i].contours.size());
    }
    std::remove(path.c_str());
}

TEST(ContourIo, RejectsForeignJson) {
    EXPECT_THROW(contours_from_json("{\"format\": \"something-else\"}"), std::runtime_error);
    EXPECT_THROW(contours_from_json("not json"), std::runtime_error);
}
