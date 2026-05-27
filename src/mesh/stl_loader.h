#ifndef MESHSLICE_STL_LOADER_H
#define MESHSLICE_STL_LOADER_H

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "mesh/mesh.h"

namespace meshslice {

class StlError : public std::runtime_error {
   public:
    explicit StlError(const std::string& msg) : std::runtime_error(msg) {
    }
};

// Parse a binary STL from an in-memory buffer. Throws StlError on malformed input
// (truncated header, truncated triangle records, count overflow).
Mesh parse_binary_stl(const uint8_t* data, size_t size);

// Convenience wrapper that reads the whole file then parses it.
Mesh load_binary_stl(const std::string& path);

// Serialize a mesh to binary STL bytes (used by tests and the streaming generator).
std::vector<uint8_t> write_binary_stl(const Mesh& mesh);

}  // namespace meshslice

#endif  // MESHSLICE_STL_LOADER_H
