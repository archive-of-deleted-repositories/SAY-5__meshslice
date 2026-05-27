#include "pipeline.h"

#include <array>
#include <cstdio>
#include <map>
#include <sstream>
#include <thread>

#include "ops/bbox_stats.h"
#include "ops/decimate.h"
#include "ops/normals.h"
#include "ops/voxel_count.h"
#include "partition/grid.h"
#include "pool/thread_pool.h"

namespace meshslice {

bool parse_op(const std::string& name, Op& out) {
    if (name == "decimate") {
        out = Op::Decimate;
    } else if (name == "normals") {
        out = Op::Normals;
    } else if (name == "bbox_stats") {
        out = Op::BboxStats;
    } else if (name == "voxel_count") {
        out = Op::VoxelCount;
    } else {
        return false;
    }
    return true;
}

const char* op_name(Op op) {
    switch (op) {
        case Op::Decimate:
            return "decimate";
        case Op::Normals:
            return "normals";
        case Op::BboxStats:
            return "bbox_stats";
        case Op::VoxelCount:
            return "voxel_count";
    }
    return "unknown";
}

PipelineResult run_pipeline(const Mesh& mesh, const PipelineConfig& cfg) {
    PipelineResult result;
    result.op = cfg.op;

    GridPartitioner partitioner(cfg.divisions);
    const std::vector<WorkUnit> units = partitioner.partition(mesh);
    result.partitions = units.size();

    size_t threads = cfg.threads ? cfg.threads : std::thread::hardware_concurrency();
    if (threads == 0) {
        threads = 1;
    }
    const size_t in_flight = cfg.max_in_flight ? cfg.max_in_flight : threads;
    ThreadPool pool(threads, in_flight);

    const Aabb global = mesh.bounds();

    switch (cfg.op) {
        case Op::BboxStats: {
            std::vector<BboxStats> per(units.size());
            pool.run(units.size(), [&](size_t i) { per[i] = bbox_stats_unit(mesh, units[i]); });
            MergedBbox m = merge_bbox(per);
            result.bbox = m;
            // Hash a canonical byte image of the merged stats.
            std::array<double, 11> img{m.box.min.x,
                                       m.box.min.y,
                                       m.box.min.z,
                                       m.box.max.x,
                                       m.box.max.y,
                                       m.box.max.z,
                                       m.area_weighted_centroid.x,
                                       m.area_weighted_centroid.y,
                                       m.area_weighted_centroid.z,
                                       m.surface_area,
                                       static_cast<double>(m.triangles)};
            result.output_hash = hash_bytes(img.data(), sizeof(img));
            break;
        }
        case Op::VoxelCount: {
            std::vector<VoxelResult> per(units.size());
            pool.run(units.size(), [&](size_t i) {
                per[i] = voxel_count_unit(mesh, units[i], global, cfg.voxel_resolution);
            });
            const uint64_t n = merge_voxels(per, cfg.voxel_resolution);
            result.voxel_count = n;
            result.output_hash = hash_bytes(&n, sizeof(n));
            break;
        }
        case Op::Normals: {
            std::vector<NormalResult> per(units.size());
            pool.run(units.size(), [&](size_t i) { per[i] = normals_unit(mesh, units[i]); });
            std::map<uint32_t, Vec3> merged = merge_normals(per);
            result.normal_vertices = merged.size();
            // Canonical byte image: (vid, nx, ny, nz) in ascending vid order.
            std::vector<uint8_t> img;
            img.reserve(merged.size() * (sizeof(uint32_t) + 3 * sizeof(float)));
            for (const auto& [vid, n] : merged) {
                const uint8_t* vp = reinterpret_cast<const uint8_t*>(&vid);
                img.insert(img.end(), vp, vp + sizeof(vid));
                for (float comp : {n.x, n.y, n.z}) {
                    const uint8_t* cp = reinterpret_cast<const uint8_t*>(&comp);
                    img.insert(img.end(), cp, cp + sizeof(comp));
                }
            }
            result.output_hash = hash_bytes(img.data(), img.size());
            break;
        }
        case Op::Decimate: {
            std::vector<DecimateResult> per(units.size());
            pool.run(units.size(),
                     [&](size_t i) { per[i] = decimate_unit(mesh, units[i], cfg.decimate_ratio); });
            std::vector<Triangle> merged = merge_decimate(per);
            result.decimated_triangles = merged.size();
            std::vector<uint8_t> img;
            img.reserve(merged.size() * 3 * sizeof(uint32_t));
            for (const Triangle& t : merged) {
                for (uint32_t v : t.v) {
                    const uint8_t* vp = reinterpret_cast<const uint8_t*>(&v);
                    img.insert(img.end(), vp, vp + sizeof(v));
                }
            }
            result.output_hash = hash_bytes(img.data(), img.size());
            break;
        }
    }

    result.peak_in_flight = pool.peak_in_flight();
    return result;
}

std::string format_result(const PipelineResult& r) {
    std::ostringstream os;
    os << "op=" << op_name(r.op) << "\n";
    os << "partitions=" << r.partitions << "\n";
    char buf[64];
    switch (r.op) {
        case Op::BboxStats:
            os << "triangles=" << r.bbox.triangles << "\n";
            std::snprintf(buf, sizeof(buf), "%.4f", r.bbox.surface_area);
            os << "surface_area=" << buf << "\n";
            std::snprintf(buf, sizeof(buf), "%.4f %.4f %.4f", r.bbox.box.min.x, r.bbox.box.min.y,
                          r.bbox.box.min.z);
            os << "aabb_min=" << buf << "\n";
            std::snprintf(buf, sizeof(buf), "%.4f %.4f %.4f", r.bbox.box.max.x, r.bbox.box.max.y,
                          r.bbox.box.max.z);
            os << "aabb_max=" << buf << "\n";
            std::snprintf(buf, sizeof(buf), "%.4f %.4f %.4f", r.bbox.area_weighted_centroid.x,
                          r.bbox.area_weighted_centroid.y, r.bbox.area_weighted_centroid.z);
            os << "centroid=" << buf << "\n";
            break;
        case Op::VoxelCount:
            os << "occupied_voxels=" << r.voxel_count << "\n";
            break;
        case Op::Normals:
            os << "normal_vertices=" << r.normal_vertices << "\n";
            break;
        case Op::Decimate:
            os << "decimated_triangles=" << r.decimated_triangles << "\n";
            break;
    }
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(r.output_hash));
    os << "output_hash=" << buf << "\n";
    return os.str();
}

}  // namespace meshslice
