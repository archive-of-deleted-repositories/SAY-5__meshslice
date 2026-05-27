#include <gtest/gtest.h>

#include <vector>

#include "merge/merger.h"

using namespace meshslice;

TEST(Merger, BboxCombinesPartitions) {
    std::vector<BboxStats> parts(2);
    parts[0].box.expand(Vec3(0, 0, 0));
    parts[0].box.expand(Vec3(1, 1, 1));
    parts[0].surface_area = 2.0;
    parts[0].triangles = 3;
    parts[0].area_weighted_centroid = Vec3(0.5f, 0.5f, 0.5f);

    parts[1].box.expand(Vec3(2, 2, 2));
    parts[1].box.expand(Vec3(3, 3, 3));
    parts[1].surface_area = 2.0;
    parts[1].triangles = 4;
    parts[1].area_weighted_centroid = Vec3(2.5f, 2.5f, 2.5f);

    const MergedBbox m = merge_bbox(parts);
    EXPECT_EQ(m.triangles, 7u);
    EXPECT_NEAR(m.surface_area, 4.0, 1e-9);
    EXPECT_NEAR(m.box.min.x, 0.0f, 1e-6);
    EXPECT_NEAR(m.box.max.x, 3.0f, 1e-6);
    EXPECT_NEAR(m.area_weighted_centroid.x, 1.5f, 1e-5);  // equal weights -> midpoint.
}

TEST(Merger, NormalsAccumulateAcrossPartitions) {
    std::vector<NormalResult> parts(2);
    parts[0].accum[7] = Vec3(0, 0, 1);
    parts[1].accum[7] = Vec3(0, 0, 3);
    const auto merged = merge_normals(parts);
    ASSERT_EQ(merged.count(7), 1u);
    EXPECT_NEAR(merged.at(7).z, 1.0f, 1e-6);  // summed then normalized.
}

TEST(Merger, VoxelsDeduplicateSharedCells) {
    std::vector<VoxelResult> parts(2);
    parts[0].occupied = {1, 2, 3};
    parts[1].occupied = {3, 4};
    EXPECT_EQ(merge_voxels(parts, 8), 4u);
}

TEST(Merger, DecimateConcatenatesInOrder) {
    std::vector<DecimateResult> parts(2);
    parts[0].kept = {Triangle{{0, 1, 2}}};
    parts[1].kept = {Triangle{{3, 4, 5}}, Triangle{{6, 7, 8}}};
    const auto out = merge_decimate(parts);
    ASSERT_EQ(out.size(), 3u);
    EXPECT_EQ(out[0].v[0], 0u);
    EXPECT_EQ(out[2].v[0], 6u);
}

TEST(Merger, HashIsOrderSensitive) {
    const uint32_t a[3] = {1, 2, 3};
    const uint32_t b[3] = {3, 2, 1};
    EXPECT_NE(hash_bytes(a, sizeof(a)), hash_bytes(b, sizeof(b)));
}
