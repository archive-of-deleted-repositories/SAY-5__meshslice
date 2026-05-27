#include <gtest/gtest.h>

#include "mesh/obj_loader.h"

using namespace meshslice;

TEST(ObjLoader, SingleTriangle) {
    const Mesh m = parse_obj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    ASSERT_EQ(m.triangle_count(), 1u);
    EXPECT_EQ(m.triangles[0].v[0], 0u);
    EXPECT_EQ(m.triangles[0].v[2], 2u);
}

TEST(ObjLoader, QuadIsFanTriangulated) {
    const Mesh m = parse_obj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n");
    EXPECT_EQ(m.triangle_count(), 2u);
}

TEST(ObjLoader, IgnoresCommentsAndOtherDirectives) {
    const Mesh m = parse_obj("# comment\nvn 0 0 1\nv 0 0 0\nv 1 0 0\nv 0 1 0\ns 1\nf 1 2 3\n");
    EXPECT_EQ(m.triangle_count(), 1u);
}

TEST(ObjLoader, FaceWithTextureNormalIndices) {
    const Mesh m = parse_obj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1/1/1 2/2/2 3/3/3\n");
    ASSERT_EQ(m.triangle_count(), 1u);
    EXPECT_EQ(m.triangles[0].v[1], 1u);
}

TEST(ObjLoader, NegativeRelativeIndices) {
    const Mesh m = parse_obj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf -3 -2 -1\n");
    ASSERT_EQ(m.triangle_count(), 1u);
    EXPECT_EQ(m.triangles[0].v[0], 0u);
}

TEST(ObjLoader, OutOfRangeIndexThrows) {
    EXPECT_THROW(parse_obj("v 0 0 0\nf 1 2 3\n"), ObjError);
}
