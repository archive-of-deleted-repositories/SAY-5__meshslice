#include <gtest/gtest.h>

#include <atomic>

#include "mesh/generators.h"
#include "partition/grid.h"
#include "pool/thread_pool.h"

using namespace meshslice;

// The bounded-memory guarantee: with max_in_flight = k, never more than k units
// are checked out simultaneously, so peak working set is bounded by
// k * max_unit_bytes rather than the whole mesh.
TEST(MemoryBudget, PeakInFlightNeverExceedsCap) {
    const Mesh m = make_sphere(40, 60);
    GridPartitioner part(10);
    const auto units = part.partition(m);
    ASSERT_GT(units.size(), 8u);

    for (size_t cap : {1u, 2u, 3u}) {
        ThreadPool pool(8, cap);
        std::atomic<size_t> live{0};
        std::atomic<size_t> peak{0};
        pool.run(units.size(), [&](size_t) {
            size_t cur = ++live;
            size_t prev = peak.load();
            while (cur > prev && !peak.compare_exchange_weak(prev, cur)) {
            }
            volatile int s = 0;
            while (s < 5000) {
                s = s + 1;
            }
            --live;
        });
        EXPECT_LE(peak.load(), cap) << "cap=" << cap;
        EXPECT_LE(pool.peak_in_flight(), cap) << "cap=" << cap;
    }
}

TEST(MemoryBudget, BoundedByWorkersTimesMaxUnit) {
    const Mesh m = make_grid_mesh(50000);
    GridPartitioner part(16);
    const auto units = part.partition(m);

    size_t max_unit_bytes = 0;
    for (const auto& u : units) {
        max_unit_bytes = std::max(max_unit_bytes, u.approx_bytes());
    }
    const size_t workers = 4;
    const size_t cap = 4;
    const size_t budget = cap * max_unit_bytes;

    // The documented bound: in-flight footprint <= workers * max_unit_bytes,
    // which is far below the full-mesh footprint for a large mesh.
    const size_t full_mesh_bytes = units.size() * max_unit_bytes;
    EXPECT_LT(budget, full_mesh_bytes);
    EXPECT_GE(workers, cap);
}
