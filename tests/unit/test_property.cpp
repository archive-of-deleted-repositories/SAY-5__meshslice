#include <gtest/gtest.h>

#include <set>
#include <vector>

#include "mesh/generators.h"
#include "mesh/types.h"
#include "partition/grid.h"

using namespace meshslice;

namespace {

struct SplitMix {
    uint64_t s;
    explicit SplitMix(uint64_t seed) : s(seed) {
    }
    uint64_t next() {
        uint64_t z = (s += 0x9e3779b97f4a7c15ull);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }
    float coord() {
        return static_cast<float>(next() % 20000) / 100.0f - 100.0f;
    }
};

// Random triangle soup: independent vertices, no shared topology.
Mesh random_soup(uint64_t seed, size_t tris) {
    SplitMix rng(seed);
    Mesh m;
    for (size_t i = 0; i < tris; ++i) {
        const uint32_t base = static_cast<uint32_t>(m.vertices.size());
        for (int k = 0; k < 3; ++k) {
            m.vertices.push_back(Vec3(rng.coord(), rng.coord(), rng.coord()));
        }
        m.triangles.push_back(Triangle{{base, base + 1, base + 2}});
    }
    return m;
}

}  // namespace

// Property: across many random soups and grid sizes, the partition is always a
// disjoint cover of the input triangles.
TEST(PartitionProperty, DisjointCoverOverRandomSoups) {
    for (uint64_t seed = 1; seed <= 40; ++seed) {
        const size_t tris = 50 + (seed * 37) % 500;
        const Mesh m = random_soup(seed, tris);
        const uint32_t divs = 1 + static_cast<uint32_t>(seed % 12);
        GridPartitioner part(divs);
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
        ASSERT_EQ(total, m.triangle_count()) << "seed=" << seed << " divs=" << divs;
        for (size_t i = 0; i < seen.size(); ++i) {
            ASSERT_EQ(seen[i], 1) << "triangle " << i << " seed=" << seed;
        }
    }
}

// Property: the union of partition triangle-sets equals the full input set.
TEST(PartitionProperty, UnionEqualsInput) {
    for (uint64_t seed = 100; seed <= 130; ++seed) {
        const Mesh m = random_soup(seed, 200 + seed % 100);
        GridPartitioner part(1 + static_cast<uint32_t>(seed % 8));
        const auto units = part.partition(m);
        std::set<uint32_t> covered;
        for (const auto& u : units) {
            covered.insert(u.triangles.begin(), u.triangles.end());
        }
        ASSERT_EQ(covered.size(), m.triangle_count()) << "seed=" << seed;
    }
}
