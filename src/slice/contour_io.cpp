#include "slice/contour_io.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace meshslice {

namespace {

std::string f32(float v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.9g", static_cast<double>(v));
    return buf;
}

std::string f64(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

// Minimal cursor-based reader for the fixed document shape this module writes.
struct Reader {
    const std::string& s;
    size_t pos = 0;

    explicit Reader(const std::string& text) : s(text) {
    }

    void skip_ws() {
        while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) {
            ++pos;
        }
    }

    [[noreturn]] void fail(const std::string& what) {
        throw std::runtime_error("contours json: " + what + " at offset " + std::to_string(pos));
    }

    void expect(char ch) {
        skip_ws();
        if (pos >= s.size() || s[pos] != ch) {
            fail(std::string("expected '") + ch + "'");
        }
        ++pos;
    }

    bool consume(char ch) {
        skip_ws();
        if (pos < s.size() && s[pos] == ch) {
            ++pos;
            return true;
        }
        return false;
    }

    std::string key() {
        expect('"');
        const size_t start = pos;
        while (pos < s.size() && s[pos] != '"') {
            ++pos;
        }
        if (pos >= s.size()) {
            fail("unterminated string");
        }
        const std::string out = s.substr(start, pos - start);
        ++pos;
        expect(':');
        return out;
    }

    std::string string_value() {
        expect('"');
        const size_t start = pos;
        while (pos < s.size() && s[pos] != '"') {
            ++pos;
        }
        if (pos >= s.size()) {
            fail("unterminated string");
        }
        const std::string out = s.substr(start, pos - start);
        ++pos;
        return out;
    }

    double number() {
        skip_ws();
        const char* begin = s.c_str() + pos;
        char* end = nullptr;
        const double v = std::strtod(begin, &end);
        if (end == begin) {
            fail("expected number");
        }
        pos += static_cast<size_t>(end - begin);
        return v;
    }

    bool boolean() {
        skip_ws();
        if (s.compare(pos, 4, "true") == 0) {
            pos += 4;
            return true;
        }
        if (s.compare(pos, 5, "false") == 0) {
            pos += 5;
            return false;
        }
        fail("expected boolean");
    }
};

Contour read_contour(Reader& r) {
    Contour c;
    r.expect('{');
    do {
        const std::string k = r.key();
        if (k == "closed") {
            c.closed = r.boolean();
        } else if (k == "perimeter") {
            c.perimeter = r.number();
        } else if (k == "signed_area") {
            c.signed_area = r.number();
        } else if (k == "points") {
            r.expect('[');
            if (!r.consume(']')) {
                do {
                    r.expect('[');
                    Vec3 p;
                    p.x = static_cast<float>(r.number());
                    r.expect(',');
                    p.y = static_cast<float>(r.number());
                    r.expect(',');
                    p.z = static_cast<float>(r.number());
                    r.expect(']');
                    c.points.push_back(p);
                } while (r.consume(','));
                r.expect(']');
            }
        } else {
            r.fail("unknown contour key '" + k + "'");
        }
    } while (r.consume(','));
    r.expect('}');
    return c;
}

Layer read_layer(Reader& r) {
    Layer layer;
    r.expect('{');
    do {
        const std::string k = r.key();
        if (k == "z") {
            layer.z = static_cast<float>(r.number());
        } else if (k == "contours") {
            r.expect('[');
            if (!r.consume(']')) {
                do {
                    layer.contours.push_back(read_contour(r));
                } while (r.consume(','));
                r.expect(']');
            }
        } else {
            r.fail("unknown layer key '" + k + "'");
        }
    } while (r.consume(','));
    r.expect('}');
    return layer;
}

}  // namespace

std::string contours_to_json(const SliceResult& result) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"format\": \"meshslice-contours\",\n";
    out << "  \"version\": 1,\n";
    out << "  \"backend\": \"" << result.backend << "\",\n";
    out << "  \"threads\": " << result.threads_used << ",\n";
    out << "  \"layers\": [";
    for (size_t i = 0; i < result.layers.size(); ++i) {
        const Layer& layer = result.layers[i];
        out << (i ? ",\n    {" : "\n    {") << "\"z\": " << f32(layer.z) << ", \"contours\": [";
        for (size_t c = 0; c < layer.contours.size(); ++c) {
            const Contour& ct = layer.contours[c];
            out << (c ? ", {" : "{") << "\"closed\": " << (ct.closed ? "true" : "false")
                << ", \"perimeter\": " << f64(ct.perimeter)
                << ", \"signed_area\": " << f64(ct.signed_area) << ", \"points\": [";
            for (size_t p = 0; p < ct.points.size(); ++p) {
                const Vec3& pt = ct.points[p];
                out << (p ? ", [" : "[") << f32(pt.x) << ", " << f32(pt.y) << ", " << f32(pt.z)
                    << "]";
            }
            out << "]}";
        }
        out << "]}";
    }
    out << "\n  ]\n}\n";
    return out.str();
}

void write_contours(const SliceResult& result, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot open for writing: " + path);
    }
    out << contours_to_json(result);
}

SliceResult contours_from_json(const std::string& json) {
    SliceResult result;
    Reader r(json);
    r.expect('{');
    do {
        const std::string k = r.key();
        if (k == "format") {
            if (r.string_value() != "meshslice-contours") {
                r.fail("not a meshslice contours file");
            }
        } else if (k == "version") {
            if (r.number() != 1.0) {
                r.fail("unsupported version");
            }
        } else if (k == "backend") {
            result.backend = r.string_value();
        } else if (k == "threads") {
            result.threads_used = static_cast<size_t>(r.number());
        } else if (k == "layers") {
            r.expect('[');
            if (!r.consume(']')) {
                do {
                    result.layers.push_back(read_layer(r));
                } while (r.consume(','));
                r.expect(']');
            }
        } else {
            r.fail("unknown top-level key '" + k + "'");
        }
    } while (r.consume(','));
    r.expect('}');
    return result;
}

SliceResult read_contours(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open: " + path);
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    return contours_from_json(buf.str());
}

}  // namespace meshslice
