#include <cstddef>
#include <cstdint>

#include "mesh/stl_loader.h"

// libFuzzer entry point. The STL parser must never crash on arbitrary input:
// malformed buffers, truncated headers, and bogus triangle counts must surface
// as StlError (caught here) rather than memory errors.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    try {
        meshslice::Mesh mesh = meshslice::parse_binary_stl(data, size);
        // Touch the parsed data so the optimizer cannot elide the parse.
        volatile size_t sink = mesh.triangle_count() + mesh.vertex_count();
        (void)sink;
    } catch (const meshslice::StlError&) {
        // Expected for malformed input.
    }
    return 0;
}
