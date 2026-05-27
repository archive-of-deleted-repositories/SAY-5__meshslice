#include "pool/numa.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#if defined(__linux__)
#include <dirent.h>
#include <pthread.h>
#include <sched.h>
#endif

namespace meshslice {

size_t NumaTopology::total_cpus() const {
    size_t n = 0;
    for (const Node& node : nodes) {
        n += node.cpus.size();
    }
    return n;
}

namespace {

#if defined(__linux__)
// Parse a Linux cpulist string like "0-3,8,10-11" into individual ids.
std::vector<uint32_t> parse_cpulist(const std::string& text) {
    std::vector<uint32_t> out;
    std::stringstream ss(text);
    std::string token;
    while (std::getline(ss, token, ',')) {
        const size_t dash = token.find('-');
        if (dash == std::string::npos) {
            if (!token.empty()) {
                out.push_back(static_cast<uint32_t>(std::stoul(token)));
            }
        } else {
            const uint32_t lo = static_cast<uint32_t>(std::stoul(token.substr(0, dash)));
            const uint32_t hi = static_cast<uint32_t>(std::stoul(token.substr(dash + 1)));
            for (uint32_t c = lo; c <= hi; ++c) {
                out.push_back(c);
            }
        }
    }
    return out;
}
#endif  // __linux__

NumaTopology single_node_fallback() {
    NumaTopology t;
    NumaTopology::Node n;
    n.id = 0;
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) {
        hw = 1;
    }
    for (unsigned c = 0; c < hw; ++c) {
        n.cpus.push_back(c);
    }
    t.nodes.push_back(std::move(n));
    return t;
}

}  // namespace

NumaTopology detect_numa_topology() {
#if defined(__linux__)
    NumaTopology topo;
    DIR* dir = opendir("/sys/devices/system/node");
    if (dir == nullptr) {
        return single_node_fallback();
    }
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        const std::string name = ent->d_name;
        if (name.rfind("node", 0) != 0 || name.size() <= 4) {
            continue;
        }
        bool numeric = true;
        for (size_t i = 4; i < name.size(); ++i) {
            if (name[i] < '0' || name[i] > '9') {
                numeric = false;
                break;
            }
        }
        if (!numeric) {
            continue;
        }
        NumaTopology::Node node;
        node.id = static_cast<uint32_t>(std::stoul(name.substr(4)));
        std::ifstream f("/sys/devices/system/node/" + name + "/cpulist");
        if (f) {
            std::string line;
            std::getline(f, line);
            node.cpus = parse_cpulist(line);
        }
        if (!node.cpus.empty()) {
            topo.nodes.push_back(std::move(node));
        }
    }
    closedir(dir);
    if (topo.nodes.empty()) {
        return single_node_fallback();
    }
    std::sort(topo.nodes.begin(), topo.nodes.end(),
              [](const NumaTopology::Node& a, const NumaTopology::Node& b) { return a.id < b.id; });
    return topo;
#else
    return single_node_fallback();
#endif
}

std::vector<uint32_t> assign_partitions_to_nodes(const NumaTopology& topo, size_t partition_count) {
    std::vector<uint32_t> assignment(partition_count, 0);
    const size_t nodes = std::max<size_t>(1, topo.node_count());
    for (size_t i = 0; i < partition_count; ++i) {
        assignment[i] = topo.nodes.empty() ? 0u : topo.nodes[i % nodes].id;
    }
    return assignment;
}

bool pin_thread_to_node(const NumaTopology& topo, uint32_t node_id) {
#if defined(__linux__)
    const NumaTopology::Node* node = nullptr;
    for (const NumaTopology::Node& n : topo.nodes) {
        if (n.id == node_id) {
            node = &n;
            break;
        }
    }
    if (node == nullptr || node->cpus.empty()) {
        return false;
    }
    cpu_set_t set;
    CPU_ZERO(&set);
    for (uint32_t cpu : node->cpus) {
        CPU_SET(cpu, &set);
    }
    return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
#else
    (void)topo;
    (void)node_id;
    return false;  // affinity control unavailable on this platform.
#endif
}

}  // namespace meshslice
