#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "mesh/obj_loader.h"
#include "mesh/stl_loader.h"
#include "pipeline.h"
#include "pipeline_stream.h"
#include "slice/adaptive.h"
#include "slice/contour_io.h"
#include "slice/slicer.h"
#include "slice/validate.h"

namespace {

using namespace meshslice;

void print_usage() {
    std::cout << "meshslice <op> <file.stl|file.obj> [options]\n"
              << "  ops: decimate | normals | bbox_stats | voxel_count | slice | validate\n"
              << "  options:\n"
              << "    --threads N        worker threads (default: hardware)\n"
              << "    --in-flight N      max units in flight (default: threads)\n"
              << "    --divisions N      grid cells per axis (default: 8)\n"
              << "    --voxel-res N      voxel resolution (default: 64)\n"
              << "    --ratio R          decimate target ratio (default: 0.5)\n"
              << "    --numa             pin workers to NUMA nodes (default: off)\n"
              << "    --stream           out-of-core streaming mode, STL only\n"
              << "                       (bbox_stats and voxel_count)\n"
              << "  slice options:\n"
              << "    --layer-height H   layer height (default: 1.0)\n"
              << "    --format F         json | csv (default: json)\n"
              << "    --fix-normals      reorient windings before slicing\n"
              << "    --adaptive         thinner layers where the surface is flat\n"
              << "    --min-layer-height H  adaptive lower bound (default: 0.05)\n"
              << "    --max-layer-height H  adaptive upper bound (default: 0.5)\n"
              << "    --export FILE      also write full contours as json\n"
              << "  validate options:\n"
              << "    --fix-normals      reorient windings, then re-validate\n";
}

Mesh load_any(const std::string& path) {
    if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".obj") == 0) {
        return load_obj(path);
    }
    return load_binary_stl(path);
}

std::string fmt_num(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6g", v);
    return buf;
}

void print_slice_json(const SliceResult& r, float layer_height) {
    std::cout << "{\n";
    std::cout << "  \"layer_height\": " << fmt_num(layer_height) << ",\n";
    std::cout << "  \"backend\": \"" << r.backend << "\",\n";
    std::cout << "  \"threads\": " << r.threads_used << ",\n";
    std::cout << "  \"layers\": [\n";
    for (size_t i = 0; i < r.layers.size(); ++i) {
        const Layer& layer = r.layers[i];
        std::cout << "    {\"z\": " << fmt_num(layer.z) << ", \"contours\": [";
        for (size_t c = 0; c < layer.contours.size(); ++c) {
            const Contour& ct = layer.contours[c];
            std::cout << (c ? ", " : "") << "{\"closed\": " << (ct.closed ? "true" : "false")
                      << ", \"perimeter\": " << fmt_num(ct.perimeter)
                      << ", \"signed_area\": " << fmt_num(ct.signed_area)
                      << ", \"points\": " << ct.points.size() << "}";
        }
        std::cout << "]}" << (i + 1 < r.layers.size() ? "," : "") << "\n";
    }
    std::cout << "  ]\n}\n";
}

void print_slice_csv(const SliceResult& r) {
    std::cout << "layer,z,contour,closed,points,perimeter,signed_area\n";
    for (size_t i = 0; i < r.layers.size(); ++i) {
        const Layer& layer = r.layers[i];
        for (size_t c = 0; c < layer.contours.size(); ++c) {
            const Contour& ct = layer.contours[c];
            std::cout << i << "," << fmt_num(layer.z) << "," << c << "," << (ct.closed ? 1 : 0)
                      << "," << ct.points.size() << "," << fmt_num(ct.perimeter) << ","
                      << fmt_num(ct.signed_area) << "\n";
        }
    }
}

// Warn (never fail) when a mesh about to be sliced is structurally suspect.
void warn_if_invalid(const ValidationReport& v) {
    if (v.boundary_edges > 0) {
        std::cerr << "warning: mesh has " << v.boundary_loops << " open boundary loop(s) ("
                  << v.boundary_edges << " boundary edges); contours may not close\n";
    }
    if (v.non_manifold_edges > 0) {
        std::cerr << "warning: mesh has " << v.non_manifold_edges << " non-manifold edge(s)\n";
    }
    if (v.degenerate_triangles > 0) {
        std::cerr << "warning: mesh has " << v.degenerate_triangles << " degenerate triangle(s)\n";
    }
    if (v.inverted_normals > 0) {
        std::cerr << "warning: mesh has " << v.inverted_normals
                  << " inverted triangle(s); signed areas may be negative"
                  << " (use --fix-normals)\n";
    }
}

int run_slice_cli(int argc, char** argv) {
    const std::string path = argv[2];
    SliceParams params;
    AdaptiveParams adaptive;
    std::string format = "json";
    std::string export_path;
    bool do_fix_normals = false;
    bool use_adaptive = false;
    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : ""; };
        if (a == "--layer-height") {
            params.layer_height = std::strtof(next().c_str(), nullptr);
        } else if (a == "--threads") {
            params.threads = std::strtoul(next().c_str(), nullptr, 10);
        } else if (a == "--format") {
            format = next();
        } else if (a == "--fix-normals") {
            do_fix_normals = true;
        } else if (a == "--adaptive") {
            use_adaptive = true;
        } else if (a == "--min-layer-height") {
            adaptive.min_layer_height = std::strtof(next().c_str(), nullptr);
        } else if (a == "--max-layer-height") {
            adaptive.max_layer_height = std::strtof(next().c_str(), nullptr);
        } else if (a == "--export") {
            export_path = next();
        } else {
            std::cerr << "unknown option: " << a << "\n";
            return 2;
        }
    }
    if (!(params.layer_height > 0.0f)) {
        std::cerr << "error: --layer-height must be positive\n";
        return 2;
    }
    if (use_adaptive && !(adaptive.min_layer_height > 0.0f &&
                          adaptive.max_layer_height >= adaptive.min_layer_height)) {
        std::cerr << "error: adaptive bounds must satisfy 0 < min <= max\n";
        return 2;
    }
    if (format != "json" && format != "csv") {
        std::cerr << "error: --format must be json or csv\n";
        return 2;
    }
    Mesh mesh = load_any(path);
    if (do_fix_normals) {
        const size_t flipped = fix_normals(mesh);
        if (flipped > 0) {
            std::cerr << "fix-normals: reoriented " << flipped << " triangle(s)\n";
        }
    }
    warn_if_invalid(validate_mesh(mesh));
    const SliceResult r =
        use_adaptive ? slice_mesh_adaptive(mesh, params, adaptive) : slice_mesh(mesh, params);
    if (!export_path.empty()) {
        write_contours(r, export_path);
        std::cerr << "exported " << r.layers.size() << " layer(s) to " << export_path << "\n";
    }
    if (format == "csv") {
        print_slice_csv(r);
    } else {
        print_slice_json(r, params.layer_height);
    }
    return 0;
}

int run_validate_cli(int argc, char** argv) {
    const std::string path = argv[2];
    bool do_fix_normals = false;
    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--fix-normals") {
            do_fix_normals = true;
        } else {
            std::cerr << "unknown option: " << a << "\n";
            return 2;
        }
    }
    Mesh mesh = load_any(path);
    if (do_fix_normals) {
        const size_t flipped = fix_normals(mesh);
        std::cout << "fixed_normals=" << flipped << "\n";
    }
    const ValidationReport report = validate_mesh(mesh);
    std::cout << format_report(report);
    return report.clean() ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace meshslice;
    if (argc < 3) {
        print_usage();
        return (argc >= 2 && std::strcmp(argv[1], "--help") == 0) ? 0 : 2;
    }

    if (std::strcmp(argv[1], "slice") == 0 || std::strcmp(argv[1], "validate") == 0) {
        try {
            return std::strcmp(argv[1], "slice") == 0 ? run_slice_cli(argc, argv)
                                                      : run_validate_cli(argc, argv);
        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            return 1;
        }
    }

    PipelineConfig cfg;
    if (!parse_op(argv[1], cfg.op)) {
        std::cerr << "unknown op: " << argv[1] << "\n";
        print_usage();
        return 2;
    }
    const std::string path = argv[2];
    bool stream = false;

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
        } else if (a == "--numa") {
            cfg.numa_aware = true;
        } else if (a == "--stream") {
            stream = true;
        } else {
            std::cerr << "unknown option: " << a << "\n";
            return 2;
        }
    }

    try {
        if (stream) {
            // Out-of-core: never materialize the whole mesh. STL only.
            StreamConfig sc;
            sc.op = cfg.op;
            sc.divisions = cfg.divisions;
            sc.voxel_resolution = cfg.voxel_resolution;
            const StreamResult r = run_streaming(stl_file_source(path), sc);
            std::cout << "op=" << op_name(r.op) << "\n";
            std::cout << "streamed_triangles=" << r.triangles << "\n";
            std::cout << "occupied_cells=" << r.occupied_cells << "\n";
            if (r.op == Op::VoxelCount) {
                std::cout << "occupied_voxels=" << r.voxel_count << "\n";
            } else {
                std::cout << "triangles=" << r.bbox.triangles << "\n";
            }
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%016llx",
                          static_cast<unsigned long long>(r.output_hash));
            std::cout << "output_hash=" << buf << "\n";
            return 0;
        }
        const Mesh mesh = load_any(path);
        const PipelineResult r = run_pipeline(mesh, cfg);
        std::cout << format_result(r);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
