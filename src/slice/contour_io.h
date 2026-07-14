#ifndef MESHSLICE_CONTOUR_IO_H
#define MESHSLICE_CONTOUR_IO_H

#include <string>

#include "slice/slicer.h"

namespace meshslice {

// Serialize a slice result, including every contour point, as JSON. Floats are
// printed with enough digits to round-trip bit-exactly through read_contours.
void write_contours(const SliceResult& result, const std::string& path);
std::string contours_to_json(const SliceResult& result);

// Load a contours JSON file produced by write_contours. Throws
// std::runtime_error on malformed input.
SliceResult read_contours(const std::string& path);
SliceResult contours_from_json(const std::string& json);

}  // namespace meshslice

#endif  // MESHSLICE_CONTOUR_IO_H
