#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>
#include <vector>

#include "mesh/generators.h"
#include "mesh/stl_loader.h"
#include "pipeline.h"
#include "pipeline_stream.h"

#if defined(__linux__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

using namespace meshslice;

namespace {

size_t peak_rss_bytes() {
#if defined(__linux__) || defined(__APPLE__)
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) != 0) {
        return 0;
    }
#if defined(__APPLE__)
    return static_cast<size_t>(ru.ru_maxrss);
#else
    return static_cast<size_t>(ru.ru_maxrss) * 1024;
#endif
#else
    return 0;
#endif
}

// Deterministic on-the-fly triangle generator: emits `n` triangles spread across
// a 10x10 area with a small height field, matching make_grid_mesh's layout so a
// streaming run can be compared to an in-core run of the same mesh. Never stores
// more than one triangle at a time.
TriangleSource grid_source(size_t target_triangles, uint64_t seed) {
    return [target_triangles, seed](const std::function<void(const StreamTriangle&)>& sink) {
        size_t quads = target_triangles / 2;
        if (quads == 0) {
            quads = 1;
        }
        size_t nn = static_cast<size_t>(std::sqrt(static_cast<double>(quads)));
        if (nn == 0) {
            nn = 1;
        }
        // splitmix64, matching generators.cpp so the height field is identical.
        uint64_t state = seed;
        auto rnd = [&]() {
            uint64_t z = (state += 0x9e3779b97f4a7c15ull);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
            z = z ^ (z >> 31);
            return static_cast<float>(z >> 40) / static_cast<float>(1u << 24);
        };
        const float step = 10.0f / static_cast<float>(nn);
        for (size_t i = 0; i < nn; ++i) {
            for (size_t j = 0; j < nn; ++j) {
                const float x = static_cast<float>(i) * step;
                const float y = static_cast<float>(j) * step;
                const float z = 0.5f * rnd();
                const Vec3 a(x, y, z), b(x + step, y, z), c(x + step, y + step, z),
                    d(x, y + step, z);
                sink(StreamTriangle{a, b, c});
                sink(StreamTriangle{a, c, d});
            }
        }
    };
}

}  // namespace

// The streaming result must match the in-core result for the same mesh.
TEST(Streaming, MatchesInCoreBbox) {
    const Mesh m = make_grid_mesh(40000, 7);

    PipelineConfig in;
    in.op = Op::BboxStats;
    in.divisions = 8;
    in.threads = 4;
    in.max_in_flight = 4;
    const PipelineResult incore = run_pipeline(m, in);

    StreamConfig sc;
    sc.op = Op::BboxStats;
    sc.divisions = 8;
    const StreamResult streamed = run_streaming(grid_source(40000, 7), sc);

    EXPECT_EQ(streamed.triangles, m.triangle_count());
    EXPECT_EQ(streamed.output_hash, incore.output_hash);
}

TEST(Streaming, MatchesInCoreVoxelCount) {
    const Mesh m = make_grid_mesh(40000, 7);

    PipelineConfig in;
    in.op = Op::VoxelCount;
    in.divisions = 8;
    in.threads = 4;
    in.max_in_flight = 4;
    in.voxel_resolution = 64;
    const PipelineResult incore = run_pipeline(m, in);

    StreamConfig sc;
    sc.op = Op::VoxelCount;
    sc.divisions = 8;
    sc.voxel_resolution = 64;
    const StreamResult streamed = run_streaming(grid_source(40000, 7), sc);

    EXPECT_EQ(streamed.voxel_count, incore.voxel_count);
    EXPECT_EQ(streamed.output_hash, incore.output_hash);
}

// The headline out-of-core property: process a mesh far larger than the memory
// cap and verify peak RSS stays under the cap. A 2 GB-equivalent in-core mesh
// would need ~2 GB; streaming holds only per-cell aggregates plus one triangle.
TEST(Streaming, PeakMemoryStaysUnderCap) {
    // 2 GB-equivalent: an in-core StreamTriangle is 36 bytes, so ~60M triangles
    // would be ~2.1 GB if materialized. We stream them and never store the mesh.
    const uint64_t big_triangles = 60'000'000ull;
    const size_t cap_bytes = 256ull * 1024 * 1024;  // 256 MB cap.

    const size_t rss_before = peak_rss_bytes();

    StreamConfig sc;
    sc.op = Op::BboxStats;
    sc.divisions = 16;  // bounded cell state: at most 16^3 CellBbox aggregates.
    const StreamResult r = run_streaming(grid_source(big_triangles, 1), sc);

    const size_t rss_after = peak_rss_bytes();
    // sqrt-flooring the quad grid yields slightly fewer than the target, but the
    // count is large and never stored, which is the point.
    EXPECT_GT(r.triangles, big_triangles / 2);
    EXPECT_LE(r.triangles, big_triangles);

    // Peak state is occupied cells, far below the cap and below a full-mesh load.
    EXPECT_LE(r.occupied_cells, static_cast<size_t>(16 * 16 * 16));
    if (rss_before > 0 && rss_after > 0) {
        EXPECT_LT(rss_after, cap_bytes) << "peak RSS " << rss_after << " exceeded cap";
    }
}

// The streaming STL reader must reject truncated input exactly like the in-core
// parser, and never crash.
TEST(Streaming, RejectsTruncatedHeader) {
    std::string bytes(10, '\0');
    std::istringstream in(bytes, std::ios::binary);
    uint64_t seen = 0;
    EXPECT_THROW(stream_binary_stl(in, [&](const StreamTriangle&) { ++seen; }), StlError);
}

TEST(Streaming, ReadsValidStlFromMemory) {
    const Mesh cube = make_cube();
    const std::vector<uint8_t> bytes = write_binary_stl(cube);
    std::string s(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::istringstream in(s, std::ios::binary);
    uint64_t seen = 0;
    const uint64_t n = stream_binary_stl(in, [&](const StreamTriangle&) { ++seen; });
    EXPECT_EQ(n, cube.triangle_count());
    EXPECT_EQ(seen, cube.triangle_count());
}

TEST(Streaming, RejectsUnsupportedOperator) {
    StreamConfig sc;
    sc.op = Op::Decimate;
    EXPECT_THROW(run_streaming(grid_source(100, 1), sc), std::invalid_argument);
}
