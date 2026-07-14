#include "slice/adaptive.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace meshslice {

namespace {

// Per-band surface flatness profile over the Z range: for each bucket, the
// maximum |n_z| of any triangle overlapping it. 1 means locally horizontal
// surface (worst stair-stepping, wants thin layers); 0 means vertical wall.
std::vector<float> flatness_profile(const Mesh& mesh, const Aabb& box, size_t buckets) {
    std::vector<float> profile(buckets, 0.0f);
    const float z0 = box.min.z;
    const float span = box.max.z - box.min.z;
    if (!(span > 0.0f)) {
        return profile;
    }
    const float inv = static_cast<float>(buckets) / span;
    for (size_t t = 0; t < mesh.triangle_count(); ++t) {
        const auto c = mesh.corners(t);
        const Vec3 n = cross(c[1] - c[0], c[2] - c[0]);
        const float len = length(n);
        if (len <= 0.0f) {
            continue;
        }
        const float flat = std::fabs(n.z) / len;
        const float tz0 = std::min({c[0].z, c[1].z, c[2].z});
        const float tz1 = std::max({c[0].z, c[1].z, c[2].z});
        size_t b0 = static_cast<size_t>(std::max(0.0f, (tz0 - z0) * inv));
        size_t b1 = static_cast<size_t>(std::max(0.0f, (tz1 - z0) * inv));
        b0 = std::min(b0, buckets - 1);
        b1 = std::min(b1, buckets - 1);
        for (size_t b = b0; b <= b1; ++b) {
            profile[b] = std::max(profile[b], flat);
        }
    }
    return profile;
}

}  // namespace

std::vector<float> plan_adaptive_planes(const Mesh& mesh, const AdaptiveParams& params) {
    std::vector<float> planes;
    const Aabb box = mesh.bounds();
    if (!box.valid()) {
        return planes;
    }
    const float min_h = std::max(1e-6f, std::min(params.min_layer_height, params.max_layer_height));
    const float max_h = std::max(min_h, params.max_layer_height);
    const size_t buckets = std::max<size_t>(1, params.profile_buckets);
    const std::vector<float> profile = flatness_profile(mesh, box, buckets);

    const float z0 = box.min.z;
    const float z1 = box.max.z;
    const float span = z1 - z0;
    if (!(span > 0.0f)) {
        planes.push_back(z0);
        return planes;
    }
    const float inv = static_cast<float>(buckets) / span;

    float z = z0;
    while (z < z1 - 1e-6f * span) {
        // Sample flatness where this layer will sit; flat surface -> thin layer.
        const size_t b = std::min(buckets - 1, static_cast<size_t>(std::max(0.0f, (z - z0) * inv)));
        const float flat = profile[b];
        float h = max_h - (max_h - min_h) * flat;
        h = std::clamp(h, min_h, max_h);
        const float remaining = z1 - z;
        if (h > remaining) {
            h = remaining;  // final partial layer.
        }
        planes.push_back(z + 0.5f * h);
        z += h;
    }
    if (planes.empty()) {
        planes.push_back(z0 + 0.5f * span);
    }
    return planes;
}

SliceResult slice_mesh_adaptive(const Mesh& mesh, const SliceParams& params,
                                const AdaptiveParams& adaptive) {
    return slice_at_planes(mesh, plan_adaptive_planes(mesh, adaptive), params);
}

}  // namespace meshslice
