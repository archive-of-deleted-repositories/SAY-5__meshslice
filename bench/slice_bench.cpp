// Slicing scaling benchmark: slice a dense generated sphere at 1/2/4/8 worker
// threads and print a markdown speedup table. Layers are embarrassingly
// parallel, so this should track the pipeline bench's near-linear curve.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "mesh/generators.h"
#include "slice/slicer.h"

using namespace meshslice;

namespace {

double time_slice(const Mesh& mesh, float layer_height, size_t threads) {
    SliceParams params;
    params.layer_height = layer_height;
    params.threads = threads;
    const auto t0 = std::chrono::steady_clock::now();
    const SliceResult r = slice_mesh(mesh, params);
    const auto t1 = std::chrono::steady_clock::now();
    // Touch the result so the work cannot be optimized away.
    size_t contours = 0;
    for (const Layer& layer : r.layers) {
        contours += layer.contours.size();
    }
    if (contours == 0) {
        std::fprintf(stderr, "unexpected empty slice result\n");
        std::exit(1);
    }
    return std::chrono::duration<double>(t1 - t0).count();
}

}  // namespace

int main(int argc, char** argv) {
    uint32_t stacks = 512;
    size_t layers = 400;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--stacks") == 0 && i + 1 < argc) {
            stacks = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--layers") == 0 && i + 1 < argc) {
            layers = std::strtoul(argv[++i], nullptr, 10);
        } else {
            std::fprintf(stderr, "usage: slice_bench [--stacks N] [--layers N]\n");
            return 2;
        }
    }

    const Mesh mesh = make_sphere(stacks, stacks, 1.0f);
    const float layer_height = 2.0f / static_cast<float>(layers);
    std::printf("mesh: sphere %ux%u, %zu triangles, %zu layers\n", stacks, stacks,
                mesh.triangle_count(), layers);

    const size_t thread_counts[] = {1, 2, 4, 8};
    double base = 0.0;
    std::printf("| threads | time (s) | speedup | efficiency |\n");
    std::printf("|---------|----------|---------|------------|\n");
    for (size_t t : thread_counts) {
        // Warm-up pass, then a timed pass.
        time_slice(mesh, layer_height, t);
        const double sec = time_slice(mesh, layer_height, t);
        if (t == 1) {
            base = sec;
        }
        const double speedup = base / sec;
        std::printf("| %zu | %.3f | %.2fx | %.0f%% |\n", t, sec, speedup,
                    100.0 * speedup / static_cast<double>(t));
    }
    return 0;
}
