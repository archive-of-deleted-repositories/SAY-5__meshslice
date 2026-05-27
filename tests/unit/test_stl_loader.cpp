#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "mesh/generators.h"
#include "mesh/stl_loader.h"

using namespace meshslice;

TEST(StlLoader, RoundTripCube) {
    const Mesh cube = make_cube();
    const std::vector<uint8_t> bytes = write_binary_stl(cube);
    const Mesh back = parse_binary_stl(bytes.data(), bytes.size());
    EXPECT_EQ(back.triangle_count(), cube.triangle_count());
    EXPECT_EQ(back.vertex_count(), cube.triangle_count() * 3);
}

TEST(StlLoader, RejectsTruncatedHeader) {
    std::vector<uint8_t> bytes(10, 0);
    EXPECT_THROW(parse_binary_stl(bytes.data(), bytes.size()), StlError);
}

TEST(StlLoader, RejectsTruncatedTriangleRecords) {
    std::vector<uint8_t> bytes(84, 0);
    bytes[80] = 5;  // claim 5 triangles but provide none.
    EXPECT_THROW(parse_binary_stl(bytes.data(), bytes.size()), StlError);
}

TEST(StlLoader, ZeroTriangleHeaderIsValid) {
    std::vector<uint8_t> bytes(84, 0);
    const Mesh m = parse_binary_stl(bytes.data(), bytes.size());
    EXPECT_EQ(m.triangle_count(), 0u);
}

TEST(StlLoader, NullBufferThrows) {
    EXPECT_THROW(parse_binary_stl(nullptr, 0), StlError);
}
