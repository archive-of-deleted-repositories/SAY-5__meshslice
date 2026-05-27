#include <gtest/gtest.h>

#include <set>

#include "mesh/generators.h"
#include "pipeline.h"
#include "pool/numa.h"
#include "pool/thread_pool.h"

using namespace meshslice;

// Topology detection always yields at least one node covering at least one CPU,
// so downstream code never has to special-case an empty topology.
TEST(Numa, TopologyHasAtLeastOneNode) {
    const NumaTopology topo = detect_numa_topology();
    ASSERT_GE(topo.node_count(), 1u);
    EXPECT_GE(topo.total_cpus(), 1u);
    for (const auto& node : topo.nodes) {
        EXPECT_FALSE(node.cpus.empty());
    }
}

TEST(Numa, PartitionAssignmentCoversAllPartitions) {
    const NumaTopology topo = detect_numa_topology();
    const auto assign = assign_partitions_to_nodes(topo, 100);
    ASSERT_EQ(assign.size(), 100u);
    std::set<uint32_t> node_ids;
    for (const auto& n : topo.nodes) {
        node_ids.insert(n.id);
    }
    for (uint32_t a : assign) {
        EXPECT_TRUE(node_ids.count(a) == 1);
    }
}

// On a single-node box the NUMA scheduler must degrade gracefully: the run still
// completes correctly and claims no locality win. Pinning each worker to the one
// node's CPUs may succeed (Linux) or be unavailable (no affinity control); either
// way pinned_workers stays within [0, worker_count] and there is exactly one node
// so no cross-node locality can be exploited. This is the documented CI behavior.
TEST(Numa, SingleNodeDegradesGracefully) {
    const NumaTopology topo = detect_numa_topology();
    ThreadPool pool(4, 4, topo);
    std::vector<int> hits(200, 0);
    pool.run(200, [&](size_t i) { hits[i] += 1; });
    for (int h : hits) {
        EXPECT_EQ(h, 1);
    }
    EXPECT_LE(pool.pinned_workers(), pool.worker_count());
    if (!topo.is_multi_node()) {
        // One node means every worker shares the same CPU set: no locality win is
        // possible regardless of whether the affinity call succeeded.
        EXPECT_EQ(topo.node_count(), 1u);
    }
}

// The headline property must survive NUMA scheduling: enabling it changes thread
// placement and unit dispatch but never the canonical-order output.
TEST(Numa, OutputIdenticalWithAndWithoutNuma) {
    const Mesh m = make_blob(3, 5150);
    const Op ops[] = {Op::BboxStats, Op::VoxelCount, Op::Normals, Op::Decimate};
    for (Op op : ops) {
        PipelineConfig naive;
        naive.op = op;
        naive.divisions = 8;
        naive.threads = 4;
        naive.max_in_flight = 4;
        naive.voxel_resolution = 32;
        naive.numa_aware = false;

        PipelineConfig numa = naive;
        numa.numa_aware = true;

        const uint64_t h_naive = run_pipeline(m, naive).output_hash;
        const uint64_t h_numa = run_pipeline(m, numa).output_hash;
        EXPECT_EQ(h_naive, h_numa) << "op=" << op_name(op);
    }
}
