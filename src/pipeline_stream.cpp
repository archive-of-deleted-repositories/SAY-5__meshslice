#include "pipeline_stream.h"

#include <array>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "merge/merger.h"
#include "mesh/mesh.h"

namespace meshslice {

namespace {

// Per-cell folded aggregate for bbox_stats. Matches BboxStats so the streaming
// and in-core merges produce identical numbers.
struct CellBbox {
    Aabb box;
    Vec3 weighted{0, 0, 0};
    double surface_area = 0.0;
    uint64_t triangles = 0;
};

uint32_t axis_cell(float coord, float origin, float size, uint32_t divisions) {
    if (size <= 0.0f) {
        return 0;
    }
    long c = static_cast<long>((coord - origin) / size);
    if (c < 0) {
        c = 0;
    }
    if (c >= static_cast<long>(divisions)) {
        c = static_cast<long>(divisions) - 1;
    }
    return static_cast<uint32_t>(c);
}

uint32_t axis_voxel(float coord, float origin, float ext, uint32_t resolution) {
    if (ext <= 0.0f) {
        return 0;
    }
    long v = static_cast<long>((coord - origin) / ext * static_cast<float>(resolution));
    if (v < 0) {
        v = 0;
    }
    if (v >= static_cast<long>(resolution)) {
        v = static_cast<long>(resolution) - 1;
    }
    return static_cast<uint32_t>(v);
}

}  // namespace

StreamResult run_streaming(const TriangleSource& source, const StreamConfig& cfg) {
    if (cfg.op != Op::BboxStats && cfg.op != Op::VoxelCount) {
        throw std::invalid_argument("streaming mode supports only bbox_stats and voxel_count");
    }

    StreamResult result;
    result.op = cfg.op;

    // Pass 1: bounded-memory AABB over the whole input (a few scalars).
    Aabb global;
    uint64_t count = 0;
    source([&](const StreamTriangle& t) {
        global.expand(t.a);
        global.expand(t.b);
        global.expand(t.c);
        ++count;
    });
    result.triangles = count;
    if (count == 0) {
        return result;
    }

    const Vec3 ext = global.extent();
    const Vec3 cs(ext.x > 0 ? ext.x / cfg.divisions : 0.0f,
                  ext.y > 0 ? ext.y / cfg.divisions : 0.0f,
                  ext.z > 0 ? ext.z / cfg.divisions : 0.0f);

    if (cfg.op == Op::BboxStats) {
        // Pass 2: fold each triangle into its cell's aggregate. Peak state is one
        // CellBbox per occupied cell, not one record per triangle.
        std::map<uint64_t, CellBbox> cells;
        source([&](const StreamTriangle& t) {
            const Vec3 c = t.centroid();
            const uint32_t ci = axis_cell(c.x, global.min.x, cs.x, cfg.divisions);
            const uint32_t cj = axis_cell(c.y, global.min.y, cs.y, cfg.divisions);
            const uint32_t ck = axis_cell(c.z, global.min.z, cs.z, cfg.divisions);
            const uint64_t key =
                (static_cast<uint64_t>(ci) * cfg.divisions + cj) * cfg.divisions + ck;
            CellBbox& cell = cells[key];
            cell.box.expand(t.a);
            cell.box.expand(t.b);
            cell.box.expand(t.c);
            const float area = triangle_area(t.a, t.b, t.c);
            const Vec3 tc = (t.a + t.b + t.c) * (1.0f / 3.0f);
            cell.weighted = cell.weighted + tc * area;
            cell.surface_area += static_cast<double>(area);
            ++cell.triangles;
        });
        result.occupied_cells = cells.size();

        // Merge in canonical cell-key order, matching the in-core merger.
        std::vector<BboxStats> per;
        per.reserve(cells.size());
        for (auto& [key, cell] : cells) {
            (void)key;
            BboxStats s;
            s.box = cell.box;
            s.surface_area = cell.surface_area;
            s.triangles = cell.triangles;
            if (cell.surface_area > 0.0) {
                s.area_weighted_centroid =
                    cell.weighted * static_cast<float>(1.0 / cell.surface_area);
            }
            per.push_back(s);
        }
        const MergedBbox m = merge_bbox(per);
        result.bbox = m;
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
    } else {  // Op::VoxelCount
        // Pass 2: accumulate the global occupied-voxel set directly. Peak state is
        // the distinct-voxel set, bounded by resolution^3, not the triangle count.
        std::set<uint64_t> occupied;
        std::set<uint64_t> cellset;
        source([&](const StreamTriangle& t) {
            const Vec3 c = t.centroid();
            const uint32_t ci = axis_cell(c.x, global.min.x, cs.x, cfg.divisions);
            const uint32_t cj = axis_cell(c.y, global.min.y, cs.y, cfg.divisions);
            const uint32_t ck = axis_cell(c.z, global.min.z, cs.z, cfg.divisions);
            cellset.insert((static_cast<uint64_t>(ci) * cfg.divisions + cj) * cfg.divisions + ck);
            for (const Vec3& p : {t.a, t.b, t.c}) {
                const uint64_t vi = axis_voxel(p.x, global.min.x, ext.x, cfg.voxel_resolution);
                const uint64_t vj = axis_voxel(p.y, global.min.y, ext.y, cfg.voxel_resolution);
                const uint64_t vk = axis_voxel(p.z, global.min.z, ext.z, cfg.voxel_resolution);
                occupied.insert((vi * cfg.voxel_resolution + vj) * cfg.voxel_resolution + vk);
            }
        });
        result.occupied_cells = cellset.size();
        result.voxel_count = occupied.size();
        result.output_hash = hash_bytes(&result.voxel_count, sizeof(result.voxel_count));
    }

    return result;
}

TriangleSource stl_file_source(const std::string& path, size_t chunk_triangles) {
    return [path, chunk_triangles](const std::function<void(const StreamTriangle&)>& sink) {
        stream_binary_stl_file(path, sink, chunk_triangles);
    };
}

}  // namespace meshslice
