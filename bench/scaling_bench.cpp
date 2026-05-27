#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "mesh/generators.h"
#include "partition/grid.h"
#include "pipeline.h"

#if defined(__linux__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

using namespace meshslice;

namespace {

struct Sample {
    std::string op;
    size_t threads;
    double seconds;
    double tris_per_sec;
    double efficiency;  // speedup / threads, vs single-thread baseline.
};

// Peak resident set size in bytes (0 if unsupported on this platform).
size_t peak_rss_bytes() {
#if defined(__linux__) || defined(__APPLE__)
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) != 0) {
        return 0;
    }
#if defined(__APPLE__)
    return static_cast<size_t>(ru.ru_maxrss);  // bytes on macOS.
#else
    return static_cast<size_t>(ru.ru_maxrss) * 1024;  // kilobytes on Linux.
#endif
#else
    return 0;
#endif
}

// The bounded-memory budget: workers * largest per-unit footprint, contrasted
// with the full-mesh footprint a naive materialize-everything design would hold.
struct MemoryBudget {
    size_t max_unit_bytes = 0;
    size_t partitions = 0;
    size_t budget_bytes = 0;     // workers * max_unit_bytes.
    size_t full_mesh_bytes = 0;  // partitions * max_unit_bytes.
    size_t workers = 0;
};

MemoryBudget compute_budget(const Mesh& m, uint32_t divisions, size_t workers) {
    GridPartitioner part(divisions);
    const auto units = part.partition(m);
    MemoryBudget b;
    b.workers = workers;
    b.partitions = units.size();
    for (const auto& u : units) {
        b.max_unit_bytes = std::max(b.max_unit_bytes, u.approx_bytes());
    }
    b.budget_bytes = workers * b.max_unit_bytes;
    b.full_mesh_bytes = b.partitions * b.max_unit_bytes;
    return b;
}

double time_run(const Mesh& m, Op op, size_t threads) {
    PipelineConfig cfg;
    cfg.op = op;
    cfg.divisions = 16;
    cfg.threads = threads;
    cfg.max_in_flight = threads;
    cfg.voxel_resolution = 64;
    cfg.decimate_ratio = 0.5f;
    // Warm up once, then take the best of three to reduce noise.
    run_pipeline(m, cfg);
    double best = 1e30;
    for (int i = 0; i < 3; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        run_pipeline(m, cfg);
        const auto t1 = std::chrono::steady_clock::now();
        best = std::min(best, std::chrono::duration<double>(t1 - t0).count());
    }
    return best;
}

std::vector<Sample> run_all(const Mesh& m) {
    const Op ops[] = {Op::Decimate, Op::Normals, Op::BboxStats, Op::VoxelCount};
    size_t hw = std::thread::hardware_concurrency();
    std::vector<size_t> counts = {1, 2, 4, 8};
    std::vector<Sample> out;
    for (Op op : ops) {
        double base_time = 0;
        for (size_t tc : counts) {
            if (tc > 1 && tc > hw * 2) {
                continue;  // skip wildly oversubscribed counts on tiny CI boxes.
            }
            const double s = time_run(m, op, tc);
            if (tc == 1) {
                base_time = s;
            }
            Sample sm;
            sm.op = op_name(op);
            sm.threads = tc;
            sm.seconds = s;
            sm.tris_per_sec = static_cast<double>(m.triangle_count()) / s;
            sm.efficiency = (base_time > 0) ? (base_time / s) / static_cast<double>(tc) : 0.0;
            out.push_back(sm);
        }
    }
    return out;
}

void write_json(const std::vector<Sample>& s, const std::string& path, size_t triangles) {
    std::ostringstream os;
    os << "{\n  \"triangles\": " << triangles << ",\n  \"samples\": [\n";
    for (size_t i = 0; i < s.size(); ++i) {
        os << "    {\"op\": \"" << s[i].op << "\", \"threads\": " << s[i].threads
           << ", \"seconds\": " << s[i].seconds << ", \"tris_per_sec\": " << s[i].tris_per_sec
           << ", \"efficiency\": " << s[i].efficiency << "}";
        os << (i + 1 < s.size() ? ",\n" : "\n");
    }
    os << "  ]\n}\n";
    std::ofstream f(path);
    f << os.str();
}

bool load_baseline(const std::string& path, std::vector<Sample>& out) {
    std::ifstream f(path);
    if (!f) {
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    // Minimal hand parse keyed on the fields we emit (avoids a JSON dependency).
    size_t pos = 0;
    while ((pos = content.find("\"op\":", pos)) != std::string::npos) {
        Sample s;
        size_t q1 = content.find('"', pos + 5);
        size_t q2 = content.find('"', q1 + 1);
        s.op = content.substr(q1 + 1, q2 - q1 - 1);
        auto num_after = [&](const char* key) -> double {
            size_t k = content.find(key, q2);
            size_t c = content.find(':', k);
            return std::strtod(content.c_str() + c + 1, nullptr);
        };
        s.threads = static_cast<size_t>(num_after("\"threads\":"));
        s.seconds = num_after("\"seconds\":");
        s.tris_per_sec = num_after("\"tris_per_sec\":");
        s.efficiency = num_after("\"efficiency\":");
        out.push_back(s);
        pos = q2 + 1;
    }
    return !out.empty();
}

}  // namespace

int main(int argc, char** argv) {
    std::string out_path;
    std::string regress_path;
    double threshold = 0.30;
    size_t triangles = 500000;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (std::strcmp(argv[i], "--regress") == 0 && i + 1 < argc) {
            regress_path = argv[++i];
        } else if (std::strcmp(argv[i], "--threshold") == 0 && i + 1 < argc) {
            threshold = std::strtod(argv[++i], nullptr);
        } else if (std::strcmp(argv[i], "--triangles") == 0 && i + 1 < argc) {
            triangles = static_cast<size_t>(std::strtoul(argv[++i], nullptr, 10));
        }
    }

    std::printf("generating synthetic mesh (~%zu triangles)...\n", triangles);
    const Mesh m = make_grid_mesh(triangles);
    std::printf("actual triangles: %zu\n", m.triangle_count());

    const std::vector<Sample> samples = run_all(m);
    std::printf("\n%-12s %8s %12s %14s %12s\n", "op", "threads", "seconds", "tris/sec",
                "efficiency");
    for (const Sample& s : samples) {
        std::printf("%-12s %8zu %12.4f %14.0f %12.2f\n", s.op.c_str(), s.threads, s.seconds,
                    s.tris_per_sec, s.efficiency);
    }

    // Memory-budget report: the in-flight cap bounds peak working set to
    // workers * max_unit_bytes, well below the full-mesh footprint.
    const size_t report_workers =
        std::min<size_t>(8, std::max<size_t>(1, std::thread::hardware_concurrency()));
    const MemoryBudget mb = compute_budget(m, 16, report_workers);
    std::printf("\nmemory budget (%zu workers, 16^3 grid):\n", mb.workers);
    std::printf("  partitions        %zu\n", mb.partitions);
    std::printf("  max_unit_bytes    %zu\n", mb.max_unit_bytes);
    std::printf("  bounded_budget    %zu  (workers * max_unit_bytes)\n", mb.budget_bytes);
    std::printf("  full_mesh_bytes   %zu  (partitions * max_unit_bytes)\n", mb.full_mesh_bytes);
    if (mb.full_mesh_bytes > 0) {
        std::printf("  budget / full     %.3f\n",
                    static_cast<double>(mb.budget_bytes) / mb.full_mesh_bytes);
    }
    std::printf("  process_peak_rss  %zu bytes\n", peak_rss_bytes());

    if (!out_path.empty()) {
        write_json(samples, out_path, m.triangle_count());
        std::printf("\nwrote %s\n", out_path.c_str());
    }

    if (!regress_path.empty()) {
        std::vector<Sample> base;
        if (!load_baseline(regress_path, base)) {
            std::printf("no baseline at %s; nothing to compare\n", regress_path.c_str());
            return 0;
        }
        // The gate compares scaling efficiency (speedup / threads), which is
        // host-independent: a slower CI box has lower absolute throughput but the
        // same parallel efficiency, so the committed baseline stays meaningful
        // across machines. Single-thread rows (efficiency == 1.0 by definition)
        // are skipped.
        const size_t cores = std::max<size_t>(1, std::thread::hardware_concurrency());
        int failures = 0;
        for (const Sample& s : samples) {
            if (s.threads <= 1) {
                continue;
            }
            if (s.threads > cores) {
                // Oversubscribed: efficiency below the physical core count is
                // hardware-bound, not a code regression. Don't gate it.
                continue;
            }
            for (const Sample& b : base) {
                // Only gate operators whose baseline efficiency was meaningfully
                // parallel (> 0.4). Memory-bandwidth-bound operators sit near the
                // serial floor and their efficiency is noise-dominated, so gating
                // them would be flaky across hosts.
                if (b.op == s.op && b.threads == s.threads && b.efficiency > 0.4) {
                    const double drift = (b.efficiency - s.efficiency) / b.efficiency;
                    if (drift > threshold) {
                        std::printf(
                            "REGRESS %s t=%zu: efficiency %.2f vs baseline %.2f (%.1f%% drop)\n",
                            s.op.c_str(), s.threads, s.efficiency, b.efficiency, drift * 100.0);
                        ++failures;
                    }
                }
            }
        }
        if (failures > 0) {
            std::printf("bench-regress: %d regression(s) beyond %.0f%%\n", failures,
                        threshold * 100.0);
            return 1;
        }
        std::printf("bench-regress: no regressions beyond %.0f%%\n", threshold * 100.0);
    }
    return 0;
}
