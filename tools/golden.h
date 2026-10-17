/*
 * Golden-image test harness for GLdc.
 *
 * GLdc has a "software" backend that, on the desktop, normally renders the
 * submitted Tile-Accelerator (TA) poly-lists through SDL's GPU renderer. That
 * path needs a display and is not deterministic across machines/SDL versions,
 * so it is unsuitable for committed "golden" reference images.
 *
 * Instead this harness contains a tiny, fully self-contained, deterministic CPU
 * rasteriser that consumes exactly the same poly-lists the real backend does
 * (OP_LIST / PT_LIST / TR_LIST, filled by glEnd / glDrawArrays / ...). It
 * reproduces the backend's triangle-strip walking (see SceneListFinish in
 * GL/platforms/software.c) and the perspective divide it performs, then fills
 * triangles with Gouraud-interpolated vertex colour, optionally modulated by a
 * decoded texture. The result is compared against a committed PPM reference.
 *
 * Because the rasteriser lives here (not in the library) it never changes when
 * the library changes, so a diff in the rendered output reliably indicates a
 * regression in GLdc's transform / colour / clipping / submission pipeline.
 *
 * Determinism notes:
 *   - Pure float maths, pixel-centre sampling, top-left-ish fill rule.
 *   - Painter's-order compositing (matching the software backend, which does
 *     no depth buffering): later triangles overwrite earlier ones for opaque
 *     lists; the transparent list alpha-blends.
 *   - Comparison is tolerant (per-channel + mismatch-fraction thresholds) so
 *     sub-LSB rounding differences between compilers do not cause flakiness,
 *     while real geometry/colour regressions are still caught.
 *
 * Usage from a test:
 *     golden::Image img(64, 64);
 *     img.clear(0, 0, 0);
 *     ... GL draw calls ...
 *     golden::rasterize_all_lists(img);          // or rasterize_list(...)
 *     assert_true(golden::check(img, "name"));   // compares tests/goldens/name.ppm
 *
 * Set the environment variable GLDC_UPDATE_GOLDENS=1 to (re)generate references.
 * On a mismatch the actual and diff images are written next to the golden with
 * .actual.ppm / .diff.ppm suffixes for inspection.
 */
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

#include "GL/private.h"
#include "GL/platform.h"
#include "containers/aligned_vector.h"

#ifndef GLDC_GOLDEN_DIR
/* Fallback; the real value is injected by CMake as a compile definition. */
#define GLDC_GOLDEN_DIR "goldens"
#endif

namespace golden {

/* ------------------------------------------------------------------ Image */

struct Image {
    int w;
    int h;
    std::vector<uint8_t> rgb;  /* w*h*3, row-major, top row first */

    Image(int width, int height) : w(width), h(height), rgb(size_t(width) * height * 3, 0) {}

    void clear(uint8_t r, uint8_t g, uint8_t b) {
        for(size_t i = 0; i < rgb.size(); i += 3) {
            rgb[i + 0] = r;
            rgb[i + 1] = g;
            rgb[i + 2] = b;
        }
    }

    inline void put(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if(x < 0 || y < 0 || x >= w || y >= h) return;
        size_t i = (size_t(y) * w + x) * 3;
        rgb[i + 0] = r;
        rgb[i + 1] = g;
        rgb[i + 2] = b;
    }

    inline void get(int x, int y, uint8_t& r, uint8_t& g, uint8_t& b) const {
        size_t i = (size_t(y) * w + x) * 3;
        r = rgb[i + 0];
        g = rgb[i + 1];
        b = rgb[i + 2];
    }
};

/* -------------------------------------------------------------------- PPM */

inline bool write_ppm(const std::string& path, const Image& img) {
    FILE* f = fopen(path.c_str(), "wb");
    if(!f) return false;
    fprintf(f, "P6\n%d %d\n255\n", img.w, img.h);
    fwrite(img.rgb.data(), 1, img.rgb.size(), f);
    fclose(f);
    return true;
}

inline bool read_ppm(const std::string& path, Image& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if(!f) return false;
    char magic[3] = {0};
    int w = 0, h = 0, maxv = 0;
    if(fscanf(f, "%2s", magic) != 1 || strcmp(magic, "P6") != 0) { fclose(f); return false; }
    if(fscanf(f, "%d %d %d", &w, &h, &maxv) != 3) { fclose(f); return false; }
    fgetc(f);  /* single whitespace after the header */
    out = Image(w, h);
    size_t got = fread(out.rgb.data(), 1, out.rgb.size(), f);
    fclose(f);
    return got == out.rgb.size();
}

/* ------------------------------------------------------- Texture decoding */

/* Decodes one texel of a non-twiddled 16bpp DC texture to RGBA 0-255.
 * Only the formats useful for golden tests are supported; everything else
 * falls back to opaque white so the modulate path still produces vertex
 * colour. Twiddled/compressed/paletted formats should be exercised by the
 * byte-exact texture-loading unit tests instead. */
struct Texel { uint8_t r, g, b, a; };

inline Texel decode_texel(const TextureObject* tex, int x, int y) {
    Texel out = {255, 255, 255, 255};
    if(!tex || !tex->data) return out;

    const int w = tex->width;
    const uint16_t* px = (const uint16_t*) tex->data;
    uint16_t v = px[size_t(y) * w + x];

    switch(tex->internalFormat) {
        case GL_RGB565_KOS: {
            uint8_t r5 = (v >> 11) & 0x1f, g6 = (v >> 5) & 0x3f, b5 = v & 0x1f;
            out.r = (r5 << 3) | (r5 >> 2);
            out.g = (g6 << 2) | (g6 >> 4);
            out.b = (b5 << 3) | (b5 >> 2);
            out.a = 255;
        } break;
        case GL_ARGB4444_KOS: {
            uint8_t a4 = (v >> 12) & 0xf, r4 = (v >> 8) & 0xf, g4 = (v >> 4) & 0xf, b4 = v & 0xf;
            out.a = (a4 << 4) | a4;
            out.r = (r4 << 4) | r4;
            out.g = (g4 << 4) | g4;
            out.b = (b4 << 4) | b4;
        } break;
        case GL_ARGB1555_KOS: {
            uint8_t a1 = (v >> 15) & 0x1, r5 = (v >> 10) & 0x1f, g5 = (v >> 5) & 0x1f, b5 = v & 0x1f;
            out.a = a1 ? 255 : 0;
            out.r = (r5 << 3) | (r5 >> 2);
            out.g = (g5 << 3) | (g5 >> 2);
            out.b = (b5 << 3) | (b5 >> 2);
        } break;
        default:
            break;  /* unsupported -> white */
    }
    return out;
}

/* ------------------------------------------------------------- Rasteriser */

struct RVertex {
    float x, y;       /* screen space (post perspective divide) */
    float a, r, g, b; /* 0-1 */
    float u, v;       /* texture coords */
};

inline float edge(const RVertex& a, const RVertex& b, float px, float py) {
    return (px - a.x) * (b.y - a.y) - (py - a.y) * (b.x - a.x);
}

inline int wrap_coord(int c, int n) {
    c %= n;
    if(c < 0) c += n;
    return c;
}

inline void fill_triangle(Image& img, const RVertex& v0, const RVertex& v1,
                          const RVertex& v2, const TextureObject* tex, bool blend) {
    float area = edge(v0, v1, v2.x, v2.y);
    if(fabsf(area) < 1e-6f) return;  /* degenerate */
    float inv_area = 1.0f / area;

    int minx = (int) floorf(fminf(v0.x, fminf(v1.x, v2.x)));
    int maxx = (int) ceilf (fmaxf(v0.x, fmaxf(v1.x, v2.x)));
    int miny = (int) floorf(fminf(v0.y, fminf(v1.y, v2.y)));
    int maxy = (int) ceilf (fmaxf(v0.y, fmaxf(v1.y, v2.y)));

    if(minx < 0) minx = 0;
    if(miny < 0) miny = 0;
    if(maxx > img.w) maxx = img.w;
    if(maxy > img.h) maxy = img.h;

    for(int y = miny; y < maxy; ++y) {
        float py = y + 0.5f;
        for(int x = minx; x < maxx; ++x) {
            float px = x + 0.5f;

            float w0 = edge(v1, v2, px, py) * inv_area;
            float w1 = edge(v2, v0, px, py) * inv_area;
            float w2 = edge(v0, v1, px, py) * inv_area;

            /* Inside test that accepts either winding (no culling, like the
             * software backend). */
            bool inside = (w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                          (w0 <= 0 && w1 <= 0 && w2 <= 0);
            if(!inside) continue;

            float a = w0 * v0.a + w1 * v1.a + w2 * v2.a;
            float r = w0 * v0.r + w1 * v1.r + w2 * v2.r;
            float g = w0 * v0.g + w1 * v1.g + w2 * v2.g;
            float b = w0 * v0.b + w1 * v1.b + w2 * v2.b;

            if(tex) {
                float u = w0 * v0.u + w1 * v1.u + w2 * v2.u;
                float v = w0 * v0.v + w1 * v1.v + w2 * v2.v;
                int tx = wrap_coord((int) floorf(u * tex->width),  tex->width);
                int ty = wrap_coord((int) floorf(v * tex->height), tex->height);
                Texel t = decode_texel(tex, tx, ty);
                r *= t.r / 255.0f;
                g *= t.g / 255.0f;
                b *= t.b / 255.0f;
                a *= t.a / 255.0f;
            }

            a = a < 0 ? 0 : (a > 1 ? 1 : a);
            r = r < 0 ? 0 : (r > 1 ? 1 : r);
            g = g < 0 ? 0 : (g > 1 ? 1 : g);
            b = b < 0 ? 0 : (b > 1 ? 1 : b);

            uint8_t sr = (uint8_t) lrintf(r * 255.0f);
            uint8_t sg = (uint8_t) lrintf(g * 255.0f);
            uint8_t sb = (uint8_t) lrintf(b * 255.0f);

            if(blend) {
                uint8_t dr, dg, db;
                img.get(x, y, dr, dg, db);
                sr = (uint8_t) lrintf(sr * a + dr * (1.0f - a));
                sg = (uint8_t) lrintf(sg * a + dg * (1.0f - a));
                sb = (uint8_t) lrintf(sb * a + db * (1.0f - a));
            }
            img.put(x, y, sr, sg, sb);
        }
    }
}

inline RVertex to_screen(const Vertex* v) {
    /* Reproduce the software backend perspective divide (see
     * _glPerspectiveDivideVertex). Vertices in the lists are already viewport
     * transformed, so dividing x/y by w gives window coordinates. */
    float inv_w = (v->w != 0.0f) ? 1.0f / v->w : 1.0f;
    RVertex out;
    out.x = v->xyz[0] * inv_w;
    out.y = v->xyz[1] * inv_w;
    out.a = v->argb[0];
    out.r = v->argb[1];
    out.g = v->argb[2];
    out.b = v->argb[3];
    out.u = v->uv[0];
    out.v = v->uv[1];
    return out;
}

/* Rasterise a single poly list. Triangle-strip extraction mirrors
 * SceneListFinish() exactly so the same triangles are produced. */
inline void rasterize_list(Image& img, PolyList* list, const TextureObject* tex, bool blend) {
    uint32_t n = aligned_vector_size(&list->vector);
    if(n < 4) return;

    uint32_t vidx = 0;
    for(uint32_t i = 0; i < n; ++i) {
        Vertex* v = (Vertex*) aligned_vector_at(&list->vector, i);

        if((v->flags & GPU_CMD_POLYHDR) == GPU_CMD_POLYHDR) {
            vidx = 0;
            continue;
        }

        if(v->flags == GPU_CMD_VERTEX || v->flags == GPU_CMD_VERTEX_EOL) {
            ++vidx;
        }

        if(vidx > 2) {
            Vertex* a = (Vertex*) aligned_vector_at(&list->vector, i - 2);
            Vertex* b = (Vertex*) aligned_vector_at(&list->vector, i - 1);
            RVertex r0 = to_screen(a);
            RVertex r1 = to_screen(b);
            RVertex r2 = to_screen(v);
            fill_triangle(img, r0, r1, r2, tex, blend);
        }

        if(v->flags == GPU_CMD_VERTEX_EOL) {
            vidx = 0;
        }
    }
}

/* Convenience: rasterise the opaque, punch-through and transparent lists in
 * the order the backend submits them. */
inline void rasterize_all_lists(Image& img, const TextureObject* tex = NULL) {
    rasterize_list(img, &OP_LIST, tex, false);
    rasterize_list(img, &PT_LIST, tex, false);
    rasterize_list(img, &TR_LIST, tex, true);
}

/* ------------------------------------------------------------- Comparison */

inline std::string golden_path(const std::string& name, const char* suffix = "") {
    return std::string(GLDC_GOLDEN_DIR) + "/" + name + suffix + ".ppm";
}

/* Compare img against the committed golden. Returns true on match (or when a
 * golden was just (re)generated). max_channel_diff is the largest per-channel
 * absolute difference tolerated per pixel; max_bad_fraction is the fraction of
 * pixels allowed to exceed that. */
inline bool check(const Image& img, const std::string& name,
                  int max_channel_diff = 2, double max_bad_fraction = 0.005) {
    std::string path = golden_path(name);

    const char* update = getenv("GLDC_UPDATE_GOLDENS");
    Image golden(0, 0);
    bool have_golden = read_ppm(path, golden);

    if((update && update[0] == '1') || !have_golden) {
        if(!write_ppm(path, img)) {
            fprintf(stderr, "golden: failed to write %s\n", path.c_str());
            return false;
        }
        fprintf(stderr, "golden: generated reference %s\n", path.c_str());
        return true;
    }

    if(golden.w != img.w || golden.h != img.h) {
        fprintf(stderr, "golden: size mismatch for %s (%dx%d vs %dx%d)\n",
                name.c_str(), golden.w, golden.h, img.w, img.h);
        write_ppm(golden_path(name, ".actual"), img);
        return false;
    }

    size_t bad = 0;
    int worst = 0;
    Image diff(img.w, img.h);
    for(size_t i = 0; i < img.rgb.size(); i += 3) {
        int dr = abs(int(img.rgb[i + 0]) - int(golden.rgb[i + 0]));
        int dg = abs(int(img.rgb[i + 1]) - int(golden.rgb[i + 1]));
        int db = abs(int(img.rgb[i + 2]) - int(golden.rgb[i + 2]));
        int d = dr > dg ? (dr > db ? dr : db) : (dg > db ? dg : db);
        if(d > worst) worst = d;
        if(d > max_channel_diff) {
            ++bad;
            diff.rgb[i + 0] = 255;  /* highlight differing pixels in red */
        }
    }

    size_t total = img.rgb.size() / 3;
    double frac = double(bad) / double(total);
    if(frac > max_bad_fraction) {
        fprintf(stderr,
                "golden: MISMATCH %s — %zu/%zu px differ (%.3f%%), worst channel diff %d\n",
                name.c_str(), bad, total, frac * 100.0, worst);
        write_ppm(golden_path(name, ".actual"), img);
        write_ppm(golden_path(name, ".diff"), diff);
        return false;
    }
    return true;
}

} // namespace golden
