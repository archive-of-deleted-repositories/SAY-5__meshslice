#include <gtest/gtest.h>

#include <atomic>
#include <numeric>
#include <vector>

#include "pool/thread_pool.h"

using namespace meshslice;

TEST(ThreadPool, RunsEveryIndexExactlyOnce) {
    ThreadPool pool(4, 4);
    const size_t n = 1000;
    std::vector<int> hits(n, 0);
    pool.run(n, [&](size_t i) { hits[i] += 1; });
    for (int h : hits) {
        EXPECT_EQ(h, 1);
    }
}

TEST(ThreadPool, ComputesCorrectAggregate) {
    ThreadPool pool(8, 4);
    const size_t n = 10000;
    std::vector<uint64_t> partial(n, 0);
    pool.run(n, [&](size_t i) { partial[i] = i; });
    const uint64_t sum = std::accumulate(partial.begin(), partial.end(), uint64_t{0});
    EXPECT_EQ(sum, (n - 1) * n / 2);
}

TEST(ThreadPool, RespectsInFlightCap) {
    const size_t cap = 2;
    ThreadPool pool(8, cap);
    std::atomic<size_t> concurrent{0};
    std::atomic<size_t> observed_peak{0};
    pool.run(200, [&](size_t) {
        size_t cur = ++concurrent;
        size_t prev = observed_peak.load();
        while (cur > prev && !observed_peak.compare_exchange_weak(prev, cur)) {
        }
        volatile int spin = 0;
        while (spin < 10000) {
            spin = spin + 1;
        }
        --concurrent;
    });
    EXPECT_LE(observed_peak.load(), cap);
    EXPECT_LE(pool.peak_in_flight(), cap);
}

TEST(ThreadPool, ReusableAcrossBatches) {
    ThreadPool pool(4, 4);
    for (int round = 0; round < 5; ++round) {
        std::vector<int> hits(50, 0);
        pool.run(50, [&](size_t i) { hits[i] += 1; });
        for (int h : hits) {
            EXPECT_EQ(h, 1);
        }
    }
}

TEST(ThreadPool, EmptyBatchIsNoOp) {
    ThreadPool pool(4, 4);
    pool.run(0, [](size_t) { FAIL() << "should not run"; });
    SUCCEED();
}
