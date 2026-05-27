#ifndef MESHSLICE_NUMA_H
#define MESHSLICE_NUMA_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace meshslice {

// A view of the host NUMA topology, parsed from /sys/devices/system/node/ on
// Linux. On non-Linux hosts, or a single-socket box, this reports a single node
// covering all online CPUs so every NUMA code path degrades gracefully to the
// naive scheduler with no behavior change.
struct NumaTopology {
    struct Node {
        uint32_t id = 0;
        std::vector<uint32_t> cpus;  // logical CPU ids belonging to this node.
    };
    std::vector<Node> nodes;

    size_t node_count() const {
        return nodes.size();
    }
    bool is_multi_node() const {
        return nodes.size() > 1;
    }
    size_t total_cpus() const;
};

// Detect topology. Reads /sys on Linux; returns a synthetic single node elsewhere.
NumaTopology detect_numa_topology();

// Assign each partition (by index) to a NUMA node round-robin across nodes, so
// the units a worker on a given node processes live on that node's memory. With
// one node this maps everything to node 0 (a no-op). Returns a vector sized to
// partition_count giving the node id per partition.
std::vector<uint32_t> assign_partitions_to_nodes(const NumaTopology& topo, size_t partition_count);

// Best-effort pin the calling thread to the CPUs of the given node. Returns true
// if a pin was applied. A no-op returning false on platforms without affinity
// control or for an empty node.
bool pin_thread_to_node(const NumaTopology& topo, uint32_t node_id);

}  // namespace meshslice

#endif  // MESHSLICE_NUMA_H
