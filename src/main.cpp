#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "mesh/obj_loader.h"
#include "mesh/stl_loader.h"
#include "pipeline.h"

namespace {

using namespace meshslice;

void print_usage() {
    std::cout << "meshslice <op> <file.stl|file.obj> [options]\n"
              << "  ops: decimate | normals | bbox_stats | voxel_count\n"
              << "  options:\n"
              << "    --threads N        worker threads (default: hardware)\n"
              << "    --in-flight N      max units in flight (default: threads)\n"
              << "    --divisions N      grid cells per axis (default: 8)\n"
              << "    --voxel-res N      voxel resolution (default: 64)\n"
              << "    --ratio R          decimate target ratio (default: 0.5)\n";
}

Mesh load_any(const std::string& path) {
    if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".obj") == 0) {
        return load_obj(path);
    }
    return load_binary_stl(path);
}

}  // namespace

int main(int argc, char** argv) {
    using namespace meshslice;
    if (argc < 3) {
        print_usage();
        return (argc >= 2 && std::strcmp(argv[1], "--help") == 0) ? 0 : 2;
    }

    PipelineConfig cfg;
    if (!parse_op(argv[1], cfg.op)) {
        std::cerr << "unknown op: " << argv[1] << "\n";
        print_usage();
        return 2;
    }
    const std::string path = argv[2];

    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : ""; };
        if (a == "--threads") {
            cfg.threads = std::strtoul(next().c_str(), nullptr, 10);
        } else if (a == "--in-flight") {
            cfg.max_in_flight = std::strtoul(next().c_str(), nullptr, 10);
        } else if (a == "--divisions") {
            cfg.divisions = std::strtoul(next().c_str(), nullptr, 10);
        } else if (a == "--voxel-res") {
            cfg.voxel_resolution = std::strtoul(next().c_str(), nullptr, 10);
        } else if (a == "--ratio") {
            cfg.decimate_ratio = std::strtof(next().c_str(), nullptr);
        } else {
            std::cerr << "unknown option: " << a << "\n";
            return 2;
        }
    }

    try {
        const Mesh mesh = load_any(path);
        const PipelineResult r = run_pipeline(mesh, cfg);
        std::cout << format_result(r);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
