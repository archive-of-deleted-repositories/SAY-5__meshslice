#include <gtest/gtest.h>

#include <string>

#include "mesh/generators.h"
#include "slice/adaptive.h"
#include "slice/contour_io.h"
#include "slice/slicer.h"

using namespace meshslice;

namespace {

// Serialize only the geometric payload: runtime metadata (backend, thread
// count) legitimately varies across configurations and is normalized away.
std::string canonical_json(SliceResult r) {
    r.backend = "canonical";
    r.threads_used = 0;
    return contours_to_json(r);
}

std::string slice_json(const Mesh& mesh, size_t threads) {
    SliceParams params;
    params.layer_height = 0.05f;
    params.threads = threads;
    return canonical_json(slice_mesh(mesh, params));
}

}  // namespace

TEST(SliceDeterminism, ByteIdenticalAcrossThreadCounts) {
    const Mesh blob = make_blob(3, 42);
    const std::string baseline = slice_json(blob, 1);
    EXPECT_FALSE(baseline.empty());
    for (size_t threads : {2u, 4u, 8u}) {
        EXPECT_EQ(slice_json(blob, threads), baseline) << "threads=" << threads;
    }
}

TEST(SliceDeterminism, AdaptiveByteIdenticalAcrossThreadCounts) {
    const Mesh sphere = make_sphere(48, 48, 1.0f);
    AdaptiveParams adaptive;
    adaptive.min_layer_height = 0.02f;
    adaptive.max_layer_height = 0.2f;

    std::string baseline;
    for (size_t threads : {1u, 2u, 4u, 8u}) {
        SliceParams params;
        params.threads = threads;
        const std::string json = canonical_json(slice_mesh_adaptive(sphere, params, adaptive));
        if (threads == 1) {
            baseline = json;
        } else {
            EXPECT_EQ(json, baseline) << "threads=" << threads;
        }
    }
}

TEST(SliceDeterminism, RepeatedRunsIdentical) {
    const Mesh sphere = make_sphere(32, 32, 1.0f);
    EXPECT_EQ(slice_json(sphere, 4), slice_json(sphere, 4));
}
