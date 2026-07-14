#include "slice/slicer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef MESHSLICE_HAVE_OPENMP
#include <omp.h>
#else
#include "pool/thread_pool.h"
#endif

namespace meshslice {

namespace {

// One plane crossing of a single triangle: a directed segment in the plane.
struct Segment {
    Vec3 a;
    Vec3 b;
};

// Signed distance of a vertex to the plane, with the deterministic on-plane
// rule: anything within epsilon of the plane is pushed to +epsilon (treated as
// lying just above). Every triangle sees the same classification for a shared
// vertex, so slicing exactly through a vertex still produces closed loops.
float classify(float z, float plane_z, float eps) {
    const float d = z - plane_z;
    if (std::fabs(d) < eps) {
        return eps;
    }
    return d;
}

// Interpolated crossing point on an edge whose endpoints straddle the plane.
// The edge is canonically ordered by the caller so the two triangles sharing
// it compute a bit-identical point.
Vec3 edge_crossing(const Vec3& p, float dp, const Vec3& q, float dq) {
    const float t = dp / (dp - dq);
    return p + (q - p) * t;
}

// Intersect one triangle with the plane. Returns true and writes a directed
// segment when the triangle straddles the plane. The segment is oriented so
// that, viewed from +z, the interior of an outward-wound (CCW) surface lies to
// its left, which makes stitched outer contours counter-clockwise.
bool triangle_plane_segment(const std::array<Vec3, 3>& c, float plane_z, float eps, Segment& out) {
    const float d0 = classify(c[0].z, plane_z, eps);
    const float d1 = classify(c[1].z, plane_z, eps);
    const float d2 = classify(c[2].z, plane_z, eps);
    const bool s0 = d0 > 0.0f;
    const bool s1 = d1 > 0.0f;
    const bool s2 = d2 > 0.0f;
    if (s0 == s1 && s1 == s2) {
        return false;
    }

    Vec3 pts[2];
    int n = 0;
    const int idx[3][2] = {{0, 1}, {1, 2}, {2, 0}};
    const float d[3] = {d0, d1, d2};
    const bool s[3] = {s0, s1, s2};
    for (const auto& e : idx) {
        const int i = e[0];
        const int j = e[1];
        if (s[i] == s[j]) {
            continue;
        }
        // Canonical edge order: interpolate from the lexicographically smaller
        // endpoint so both triangles sharing this edge get the same bits.
        const Vec3& p = c[i];
        const Vec3& q = c[j];
        const bool p_first =
            (p.x < q.x) || (p.x == q.x && (p.y < q.y || (p.y == q.y && p.z < q.z)));
        Vec3 x;
        if (p_first) {
            x = edge_crossing(p, d[i], q, d[j]);
        } else {
            x = edge_crossing(q, d[j], p, d[i]);
        }
        if (n < 2) {
            pts[n] = x;
        }
        ++n;
    }
    if (n != 2) {
        return false;
    }

    // Orient the segment: its direction should match cross(z_axis, face_normal)
    // projected into the plane.
    const Vec3 normal = cross(c[1] - c[0], c[2] - c[0]);
    const Vec3 want{-normal.y, normal.x, 0.0f};
    const Vec3 dir = pts[1] - pts[0];
    if (dir.x * want.x + dir.y * want.y < 0.0f) {
        out.a = pts[1];
        out.b = pts[0];
    } else {
        out.a = pts[0];
        out.b = pts[1];
    }
    return true;
}

float dist2_xy(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// Spatial hash over quantized XY so endpoint matching is near O(1).
struct EndpointIndex {
    float cell;
    std::unordered_map<uint64_t, std::vector<uint32_t>> buckets;

    explicit EndpointIndex(float c) : cell(c) {
    }

    static uint64_t key(int64_t ix, int64_t iy) {
        return (static_cast<uint64_t>(ix) << 32) ^ (static_cast<uint64_t>(iy) & 0xffffffffull);
    }

    void insert(const Vec3& p, uint32_t id) {
        const int64_t ix = static_cast<int64_t>(std::floor(p.x / cell));
        const int64_t iy = static_cast<int64_t>(std::floor(p.y / cell));
        buckets[key(ix, iy)].push_back(id);
    }

    // Visit candidates in the 3x3 neighborhood of p.
    template <typename F>
    void neighborhood(const Vec3& p, F&& fn) const {
        const int64_t ix = static_cast<int64_t>(std::floor(p.x / cell));
        const int64_t iy = static_cast<int64_t>(std::floor(p.y / cell));
        for (int64_t dx = -1; dx <= 1; ++dx) {
            for (int64_t dy = -1; dy <= 1; ++dy) {
                const auto it = buckets.find(key(ix + dx, iy + dy));
                if (it == buckets.end()) {
                    continue;
                }
                for (uint32_t id : it->second) {
                    fn(id);
                }
            }
        }
    }
};

void finish_contour(Contour& c, float stitch_eps) {
    if (c.points.size() >= 3 &&
        dist2_xy(c.points.front(), c.points.back()) <= stitch_eps * stitch_eps * 4.0f) {
        c.points.pop_back();
        c.closed = true;
    }
    double perim = 0.0;
    for (size_t i = 0; i + 1 < c.points.size(); ++i) {
        perim += std::sqrt(static_cast<double>(dist2_xy(c.points[i], c.points[i + 1])));
    }
    if (c.closed && c.points.size() >= 2) {
        perim += std::sqrt(static_cast<double>(dist2_xy(c.points.back(), c.points.front())));
    }
    c.perimeter = perim;
    if (c.closed) {
        double area2 = 0.0;
        for (size_t i = 0; i < c.points.size(); ++i) {
            const Vec3& p = c.points[i];
            const Vec3& q = c.points[(i + 1) % c.points.size()];
            area2 += static_cast<double>(p.x) * q.y - static_cast<double>(q.x) * p.y;
        }
        c.signed_area = area2 * 0.5;
    }
}

// Stitch directed segments head-to-tail into contours. Segments are consumed
// in index order, so for a fixed mesh the result is fully deterministic.
std::vector<Contour> stitch(const std::vector<Segment>& segs, float plane_z, float stitch_eps) {
    const size_t n = segs.size();
    EndpointIndex heads(std::max(stitch_eps, 1e-12f));
    for (size_t i = 0; i < n; ++i) {
        heads.insert(segs[i].a, static_cast<uint32_t>(i));
    }

    std::vector<bool> used(n, false);
    const float eps2 = stitch_eps * stitch_eps;
    std::vector<Contour> out;

    for (size_t start = 0; start < n; ++start) {
        if (used[start]) {
            continue;
        }
        used[start] = true;
        Contour c;
        c.points.push_back(segs[start].a);
        c.points.push_back(segs[start].b);

        // Walk forward: find an unused segment whose head matches our tail.
        bool extended = true;
        while (extended) {
            extended = false;
            const Vec3& tail = c.points.back();
            uint32_t best = 0;
            float best_d = eps2;
            bool found = false;
            heads.neighborhood(tail, [&](uint32_t id) {
                if (used[id]) {
                    return;
                }
                const float d = dist2_xy(segs[id].a, tail);
                if (d <= best_d && (!found || d < best_d || id < best)) {
                    best = id;
                    best_d = d;
                    found = true;
                }
            });
            if (found) {
                used[best] = true;
                c.points.push_back(segs[best].b);
                extended = true;
                if (dist2_xy(c.points.back(), c.points.front()) <= eps2) {
                    break;
                }
            }
        }

        for (Vec3& p : c.points) {
            p.z = plane_z;
        }
        finish_contour(c, stitch_eps);
        // Discard stitching residue: a lone segment is not a contour.
        if (c.points.size() >= 3 || !c.closed) {
            out.push_back(std::move(c));
        }
    }
    return out;
}

}  // namespace

std::vector<Contour> slice_plane(const Mesh& mesh, float plane_z, const SliceParams& params) {
    std::vector<Segment> segs;
    for (size_t t = 0; t < mesh.triangle_count(); ++t) {
        Segment s;
        if (triangle_plane_segment(mesh.corners(t), plane_z, params.on_plane_epsilon, s)) {
            // Skip zero-length residue produced by near-vertex crossings.
            if (dist2_xy(s.a, s.b) > 0.0f) {
                segs.push_back(s);
            }
        }
    }
    return stitch(segs, plane_z, params.stitch_epsilon);
}

size_t layer_count_for_range(float z_min, float z_max, float layer_height) {
    if (!(layer_height > 0.0f) || z_max <= z_min) {
        return 1;
    }
    const double span = static_cast<double>(z_max) - static_cast<double>(z_min);
    const double n = std::floor(span / static_cast<double>(layer_height) + 0.5);
    return static_cast<size_t>(std::max(1.0, n));
}

SliceResult slice_mesh(const Mesh& mesh, const SliceParams& params) {
    SliceResult result;
    size_t threads = params.threads;
    if (threads == 0) {
        threads = std::max(1u, std::thread::hardware_concurrency());
    }
    result.threads_used = threads;

    const Aabb box = mesh.bounds();
    if (!box.valid()) {
#ifdef MESHSLICE_HAVE_OPENMP
        result.backend = "openmp";
#else
        result.backend = "thread_pool";
#endif
        return result;
    }

    const size_t n = layer_count_for_range(box.min.z, box.max.z, params.layer_height);
    const double span = static_cast<double>(box.max.z) - static_cast<double>(box.min.z);
    const double step =
        (span > 0.0) ? span / static_cast<double>(n) : static_cast<double>(params.layer_height);

    result.layers.resize(n);
    for (size_t i = 0; i < n; ++i) {
        result.layers[i].z = static_cast<float>(box.min.z + (static_cast<double>(i) + 0.5) * step);
    }

    // Each layer is independent; results land in per-index slots so the output
    // is identical regardless of scheduling.
    auto do_layer = [&](size_t i) {
        Layer& layer = result.layers[i];
        layer.contours = slice_plane(mesh, layer.z, params);
    };

#ifdef MESHSLICE_HAVE_OPENMP
    result.backend = "openmp";
#pragma omp parallel for schedule(dynamic) num_threads(static_cast<int>(threads))
    for (int64_t i = 0; i < static_cast<int64_t>(n); ++i) {
        do_layer(static_cast<size_t>(i));
    }
#else
    result.backend = "thread_pool";
    ThreadPool pool(threads, threads);
    pool.run(n, do_layer);
#endif

    return result;
}

}  // namespace meshslice
