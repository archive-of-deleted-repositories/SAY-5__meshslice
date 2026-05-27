#include <gtest/gtest.h>

#include <set>
#include <vector>

#include "mesh/generators.h"
#include "partition/grid.h"

using namespace meshslice;

// Core partition invariant: every triangle is assigned to exactly one unit, with
// no overlap and complete coverage.
TEST(Grid, EveryTriangleAssignedExactlyOnce) {
    const Mesh m = make_sphere(16, 24);
    GridPartitioner part(8);
    const auto units = part.partition(m);

    std::vector<int> seen(m.triangle_count(), 0);
    size_t total = 0;
    for (const auto& u : units) {
        for (uint32_t t : u.triangles) {
            ASSERT_LT(t, m.triangle_count());
            ++seen[t];
            ++total;
        }
    }
    EXPECT_EQ(total, m.triangle_count());
    for (int s : seen) {
        EXPECT_EQ(s, 1);
    }
}

TEST(Grid, UnitIndicesAreContiguousAndStable) {
    const Mesh m = make_blob(2, 99);
    GridPartitioner part(6);
    const auto a = part.partition(m);
    const auto b = part.partition(m);
    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].index, static_cast<uint32_t>(i));
        EXPECT_EQ(a[i].triangles, b[i].triangles);  // run-to-run stable.
    }
}

TEST(Grid, EmptyMeshProducesNoUnits) {
    Mesh m;
    GridPartitioner part(4);
    EXPECT_TRUE(part.partition(m).empty());
}

TEST(Grid, ZeroDivisionsRejected) {
    EXPECT_THROW(GridPartitioner(0), std::invalid_argument);
}

TEST(Grid, FlatMeshPartitionsCleanly) {
    const Mesh m = make_grid_mesh(2000);  // nearly flat height field.
    GridPartitioner part(8);
    const auto units = part.partition(m);
    size_t total = 0;
    for (const auto& u : units) {
        total += u.triangle_count();
    }
    EXPECT_EQ(total, m.triangle_count());
}
