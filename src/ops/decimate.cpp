#include "ops/decimate.h"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace meshslice {

namespace {

struct Edge {
    uint32_t a;
    uint32_t b;
    float length_sq;
};

float dist_sq(const Vec3& p, const Vec3& q) {
    const Vec3 d = p - q;
    return dot(d, d);
}

}  // namespace

DecimateResult decimate_unit(const Mesh& mesh, const WorkUnit& unit, float target_ratio) {
    DecimateResult r;
    const size_t total = unit.triangle_count();
    if (total == 0) {
        return r;
    }
    target_ratio = std::clamp(target_ratio, 0.0f, 1.0f);
    const size_t target =
        std::max<size_t>(1, static_cast<size_t>(static_cast<float>(total) * target_ratio + 0.5f));

    // Local working copy of this unit's triangles (global vertex indices).
    std::vector<Triangle> tris;
    tris.reserve(total);
    for (uint32_t t : unit.triangles) {
        tris.push_back(mesh.triangles[t]);
    }

    if (target >= total) {
        r.kept = std::move(tris);
        return r;
    }

    // Collect candidate edges ordered by squared length, then collapse greedily.
    std::vector<Edge> edges;
    edges.reserve(tris.size() * 3);
    for (const Triangle& t : tris) {
        for (int k = 0; k < 3; ++k) {
            uint32_t a = t.v[k];
            uint32_t b = t.v[(k + 1) % 3];
            if (a > b) {
                std::swap(a, b);
            }
            edges.push_back({a, b, dist_sq(mesh.vertices[a], mesh.vertices[b])});
        }
    }
    // Deterministic order: shortest first, ties broken by endpoint indices.
    std::sort(edges.begin(), edges.end(), [](const Edge& x, const Edge& y) {
        if (x.length_sq != y.length_sq) {
            return x.length_sq < y.length_sq;
        }
        if (x.a != y.a) {
            return x.a < y.a;
        }
        return x.b < y.b;
    });

    // Union-find style remap: collapse b onto a.
    std::unordered_map<uint32_t, uint32_t> remap;
    auto resolve = [&](uint32_t v) {
        while (true) {
            auto it = remap.find(v);
            if (it == remap.end()) {
                return v;
            }
            v = it->second;
        }
    };

    size_t live = total;
    auto degenerate = [&](const Triangle& t) {
        const uint32_t a = resolve(t.v[0]);
        const uint32_t b = resolve(t.v[1]);
        const uint32_t c = resolve(t.v[2]);
        return a == b || b == c || a == c;
    };

    for (const Edge& e : edges) {
        if (live <= target) {
            break;
        }
        const uint32_t a = resolve(e.a);
        const uint32_t b = resolve(e.b);
        if (a == b) {
            continue;
        }
        // Tentatively collapse b -> a and count how many triangles die.
        remap[b] = a;
        size_t died = 0;
        for (const Triangle& t : tris) {
            if (degenerate(t)) {
                ++died;
            }
        }
        // died counts all currently-degenerate triangles; recompute live freshly.
        const size_t new_live = total - died;
        if (new_live == 0) {
            remap.erase(b);  // never collapse the whole unit away.
            continue;
        }
        live = new_live;
    }

    for (const Triangle& t : tris) {
        Triangle nt;
        nt.v[0] = resolve(t.v[0]);
        nt.v[1] = resolve(t.v[1]);
        nt.v[2] = resolve(t.v[2]);
        if (nt.v[0] == nt.v[1] || nt.v[1] == nt.v[2] || nt.v[0] == nt.v[2]) {
            continue;
        }
        r.kept.push_back(nt);
    }
    return r;
}

}  // namespace meshslice
