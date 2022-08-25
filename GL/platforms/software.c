#include <SDL.h>

#include <stdlib.h>
#include <string.h>

#include "../private.h"
#include "../platform.h"
#include "software.h"
#include "software/edge_equation.h"
#include "software/parameter_equation.h"

#define CLIP_DEBUG 0

static size_t AVAILABLE_VRAM = 16 * 1024 * 1024;
static Matrix4x4 MATRIX;

static SDL_Window* WINDOW = NULL;
static SDL_Renderer* RENDERER = NULL;

static uint8_t BACKGROUND_COLOR[3] = {0, 0, 0};

GPUCulling CULL_MODE = GPU_CULLING_CCW;


static VideoMode vid_mode = {
    640, 480
};

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

static void DrawTriangle(Vertex* v0, Vertex* v1, Vertex* v2) {
    // Compute triangle bounding box.

    int minX = MIN(MIN(v0->xyz[0], v1->xyz[0]), v2->xyz[0]);
    int maxX = MAX(MAX(v0->xyz[0], v1->xyz[0]), v2->xyz[0]);
    int minY = MIN(MIN(v0->xyz[1], v1->xyz[1]), v2->xyz[1]);
    int maxY = MAX(MAX(v0->xyz[1], v1->xyz[1]), v2->xyz[1]);

    // Clip to scissor rect.

    minX = MAX(minX, 0);
    maxX = MIN(maxX, vid_mode.width);
    minY = MAX(minY, 0);
    maxY = MIN(maxY, vid_mode.height);

    // Compute edge equations.

    EdgeEquation e0, e1, e2;
    EdgeEquationInit(&e0, &v0->xyz[0], &v1->xyz[0]);
    EdgeEquationInit(&e1, &v1->xyz[0], &v2->xyz[0]);
    EdgeEquationInit(&e2, &v2->xyz[0], &v0->xyz[0]);

    float area = 0.5 * (e0.c + e1.c + e2.c);

    /* This is very ugly. I don't understand the math properly
     * so I just swap the vertex order if something is back-facing
     * and we want to render it. Patches welcome! */
#define REVERSE_WINDING() \
    Vertex* tv = v0; \
    v0 = v1; \
    v1 = tv; \
    EdgeEquationInit(&e0, &v0->xyz[0], &v1->xyz[0]); \
    EdgeEquationInit(&e1, &v1->xyz[0], &v2->xyz[0]); \
    EdgeEquationInit(&e2, &v2->xyz[0], &v0->xyz[0]); \
    area = 0.5f * (e0.c + e1.c + e2.c) \

    // Check if triangle is backfacing.
    if(CULL_MODE == GPU_CULLING_CCW) {
        if(area < 0) {
            return;
        }
    } else if(CULL_MODE == GPU_CULLING_CW) {
        if(area < 0) {
            // We only draw front-facing polygons, so swap
            // the back to front and draw
            REVERSE_WINDING();
        } else {
            // Front facing, so bail
            return;
        }
    } else if(area < 0) {
        /* We're not culling, but this is backfacing, so swap vertices and edges */
        REVERSE_WINDING();
    }

    ParameterEquation r, g, b;

    ParameterEquationInit(&r, v0->bgra[2], v1->bgra[2], v2->bgra[2], &e0, &e1, &e2, area);
    ParameterEquationInit(&g, v0->bgra[1], v1->bgra[1], v2->bgra[1], &e0, &e1, &e2, area);
    ParameterEquationInit(&b, v0->bgra[0], v1->bgra[0], v2->bgra[0], &e0, &e1, &e2, area);

    // Add 0.5 to sample at pixel centers.
    for (float x = minX + 0.5f, xm = maxX + 0.5f; x <= xm; x += 1.0f)
    for (float y = minY + 0.5f, ym = maxY + 0.5f; y <= ym; y += 1.0f)
    {
      if (EdgeEquationTestPoint(&e0, x, y) && EdgeEquationTestPoint(&e1, x, y) && EdgeEquationTestPoint(&e2, x, y)) {
        int rint = ParameterEquationEvaluate(&r, x, y);
        int gint = ParameterEquationEvaluate(&g, x, y);
        int bint = ParameterEquationEvaluate(&b, x, y);
        SDL_SetRenderDrawColor(RENDERER, rint, gint, bint, 255);
        SDL_RenderDrawPoint(RENDERER, x, y);
      }
    }
}


void InitGPU(_Bool autosort, _Bool fsaa) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    WINDOW = SDL_CreateWindow(
        "GLdc",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        vid_mode.width, vid_mode.height,
        SDL_WINDOW_SHOWN
    );

    RENDERER = SDL_CreateRenderer(
        WINDOW, -1, SDL_RENDERER_ACCELERATED
    );
}

void SceneBegin() {
    SDL_SetRenderDrawColor(RENDERER, BACKGROUND_COLOR[0], BACKGROUND_COLOR[1], BACKGROUND_COLOR[2], 0);
    SDL_RenderClear(RENDERER);
}

static Vertex BUFFER[1024 * 32];
static uint32_t vertex_counter = 0;

GL_FORCE_INLINE bool glIsVertex(const float flags) {
    return flags == GPU_CMD_VERTEX_EOL || flags == GPU_CMD_VERTEX;
}

GL_FORCE_INLINE bool glIsLastVertex(const float flags) {
    return flags == GPU_CMD_VERTEX_EOL;
}


void SceneListBegin(GPUList list) {
    vertex_counter = 0;
}

GL_FORCE_INLINE void _glPerspectiveDivideVertex(Vertex* vertex, const float h) {
    const float f = 1.0f / (vertex->w);

    /* Convert to NDC and apply viewport */
    vertex->xyz[0] = __builtin_fmaf(
        VIEWPORT.hwidth, vertex->xyz[0] * f, VIEWPORT.x_plus_hwidth
    );

    vertex->xyz[1] = h - __builtin_fmaf(
        VIEWPORT.hheight, vertex->xyz[1] * f, VIEWPORT.y_plus_hheight
    );

    if(vertex->w == 1.0f) {
        vertex->xyz[2] = 1.0f / (1.0001f + vertex->xyz[2]);
    } else {
        vertex->xyz[2] = f;
    }
}

GL_FORCE_INLINE void _glSubmitHeaderOrVertex(const Vertex* v) {
#ifndef NDEBUG
    if(glIsVertex(v->flags)) {
        gl_assert(!isnan(v->xyz[2]));
        gl_assert(!isnan(v->w));
    }
#endif

#if CLIP_DEBUG
    printf("Submitting: %x (%x)\n", v, v->flags);
#endif

    BUFFER[vertex_counter++] = *v;
}

static struct {
    Vertex* v;
    int visible;
} triangle[3];

static int tri_count = 0;
static int strip_count = 0;

GL_FORCE_INLINE void interpolateColour(const uint8_t* v1, const uint8_t* v2, const float t, uint8_t* out) {
    const int MASK1 = 0x00FF00FF;
    const int MASK2 = 0xFF00FF00;

    const int f2 = 256 * t;
    const int f1 = 256 - f2;

    const uint32_t a = *(uint32_t*) v1;
    const uint32_t b = *(uint32_t*) v2;

    *((uint32_t*) out) = (((((a & MASK1) * f1) + ((b & MASK1) * f2)) >> 8) & MASK1) |
            (((((a & MASK2) * f1) + ((b & MASK2) * f2)) >> 8) & MASK2);
}

GL_FORCE_INLINE void _glClipEdge(const Vertex* v1, const Vertex* v2, Vertex* vout) {
    /* Clipping time! */
    const float d0 = v1->w + v1->xyz[2];
    const float d1 = v2->w + v2->xyz[2];

    const float epsilon = (d0 < d1) ? -0.00001f : 0.00001f;

    float t = (d0 / (d0 - d1)) + epsilon;

    t = (t > 1.0f) ? 1.0f : t;
    t = (t < 0.0f) ? 0.0f : t;

    vout->xyz[0] = __builtin_fmaf(v2->xyz[0] - v1->xyz[0], t, v1->xyz[0]);
    vout->xyz[1] = __builtin_fmaf(v2->xyz[1] - v1->xyz[1], t, v1->xyz[1]);
    vout->xyz[2] = __builtin_fmaf(v2->xyz[2] - v1->xyz[2], t, v1->xyz[2]);
    vout->w = __builtin_fmaf(v2->w - v1->w, t, v1->w);

    vout->uv[0] = __builtin_fmaf(v2->uv[0] - v1->uv[0], t, v1->uv[0]);
    vout->uv[1] = __builtin_fmaf(v2->uv[1] - v1->uv[1], t, v1->uv[1]);

    interpolateColour(v1->bgra, v2->bgra, t, vout->bgra);
}

GL_FORCE_INLINE void ClearTriangle() {
    tri_count = 0;
}

GL_FORCE_INLINE void ShiftTriangle() {
    if(!tri_count) {
        return;
    }

    tri_count--;
    triangle[0] = triangle[1];
    triangle[1] = triangle[2];

#ifndef NDEBUG
    triangle[2].v = NULL;
    triangle[2].visible = false;
#endif
}

GL_FORCE_INLINE void ShiftRotateTriangle() {
    if(!tri_count) {
        return;
    }

    if(triangle[0].v < triangle[1].v) {
        triangle[0] = triangle[2];
    } else {
        triangle[1] = triangle[2];
    }

    tri_count--;
}

void SceneListSubmit(void* src, int n) {
        /* Perform perspective divide on each vertex */
    Vertex* vertex = (Vertex*) src;

    const float h = GetVideoMode()->height;

    /* If Z-clipping is disabled, just fire everything over to the buffer */
    if(!ZNEAR_CLIPPING_ENABLED) {
        for(int i = 0; i < n; ++i, ++vertex) {
            PREFETCH(vertex + 1);
            if(glIsVertex(vertex->flags)) {
                _glPerspectiveDivideVertex(vertex, h);
            }
            _glSubmitHeaderOrVertex(vertex);
        }

        return;
    }

    tri_count = 0;
    strip_count = 0;

#if CLIP_DEBUG
    printf("----\n");
#endif

    for(int i = 0; i < n; ++i, ++vertex) {
        PREFETCH(vertex + 1);

        bool is_last_in_strip = glIsLastVertex(vertex->flags);

        /* Wait until we fill the triangle */
        if(tri_count < 3) {
            if(glIsVertex(vertex->flags)) {
                triangle[tri_count].v = vertex;
                triangle[tri_count].visible = vertex->xyz[2] >= -vertex->w;
                tri_count++;
                strip_count++;
            } else {
                /* We hit a header */
                tri_count = 0;
                strip_count = 0;
                _glSubmitHeaderOrVertex(vertex);
            }

            if(tri_count < 3) {
                continue;
            }
        }

#if CLIP_DEBUG
        printf("SC: %d\n", strip_count);
#endif

        /* If we got here, then triangle contains 3 vertices */
        int visible_mask = triangle[0].visible | (triangle[1].visible << 1) | (triangle[2].visible << 2);
        if(visible_mask == 7) {
#if CLIP_DEBUG
            printf("Visible\n");
#endif
            /* All the vertices are visible! We divide and submit v0, then shift */
            _glPerspectiveDivideVertex(vertex - 2, h);
            _glSubmitHeaderOrVertex(vertex - 2);

            if(is_last_in_strip) {
                _glPerspectiveDivideVertex(vertex - 1, h);
                _glSubmitHeaderOrVertex(vertex - 1);
                _glPerspectiveDivideVertex(vertex, h);
                _glSubmitHeaderOrVertex(vertex);
                tri_count = 0;
                strip_count = 0;
            }

            ShiftRotateTriangle();

        } else if(visible_mask) {
            /* Clipping time!

                There are 6 distinct possibilities when clipping a triangle. 3 of them result
                in another triangle, 3 of them result in a quadrilateral.

                Assuming you iterate the edges of the triangle in order, and create a new *visible*
                vertex when you cross the plane, and discard vertices behind the plane, then the only
                difference between the two cases is that the final two vertices that need submitting have
                to be reversed.

                Unfortunately we have to copy vertices here, because if we persp-divide a vertex it may
                be used in a subsequent triangle in the strip and would end up being double divided.
            */
#if CLIP_DEBUG
            printf("Clip: %d, SC: %d\n", visible_mask, strip_count);
            printf("%d, %d, %d\n", triangle[0].v - (Vertex*) src - 1, triangle[1].v - (Vertex*) src - 1, triangle[2].v - (Vertex*) src - 1);
#endif
            Vertex tmp;
            if(strip_count > 3) {
#if CLIP_DEBUG
                printf("Flush\n");
#endif
                tmp = *(vertex - 2);
                /* If we had triangles ahead of this one, submit and finalize */
                _glPerspectiveDivideVertex(&tmp, h);
                _glSubmitHeaderOrVertex(&tmp);

                tmp = *(vertex - 1);
                tmp.flags = GPU_CMD_VERTEX_EOL;
                _glPerspectiveDivideVertex(&tmp, h);
                _glSubmitHeaderOrVertex(&tmp);
            }

            switch(visible_mask) {
                case 1: {
                    /* 0, 0a, 2a */
                    tmp = *triangle[0].v;
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    _glClipEdge(triangle[0].v, triangle[1].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    _glClipEdge(triangle[2].v, triangle[0].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX_EOL;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);
                } break;
                case 2: {
                    /* 0a, 1, 1a */
                    _glClipEdge(triangle[0].v, triangle[1].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    tmp = *triangle[1].v;
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    _glClipEdge(triangle[1].v, triangle[2].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX_EOL;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);
                } break;
                case 3: {
                    /* 0, 1, 2a, 1a */
                    tmp = *triangle[0].v;
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    tmp = *triangle[1].v;
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    _glClipEdge(triangle[2].v, triangle[0].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    _glClipEdge(triangle[1].v, triangle[2].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX_EOL;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);
                } break;
                case 4: {
                    /* 1a, 2, 2a */
                    _glClipEdge(triangle[1].v, triangle[2].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    tmp = *triangle[2].v;
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    _glClipEdge(triangle[2].v, triangle[0].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX_EOL;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);
                } break;
                case 5: {
                    /* 0, 0a, 2, 1a */
                    tmp = *triangle[0].v;
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    _glClipEdge(triangle[0].v, triangle[1].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    tmp = *triangle[2].v;
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    _glClipEdge(triangle[1].v, triangle[2].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX_EOL;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);
                } break;
                case 6: {
                    /* 0a, 1, 2a, 2 */
                    _glClipEdge(triangle[0].v, triangle[1].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    tmp = *triangle[1].v;
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    _glClipEdge(triangle[2].v, triangle[0].v, &tmp);
                    tmp.flags = GPU_CMD_VERTEX;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);

                    tmp = *triangle[2].v;
                    tmp.flags = GPU_CMD_VERTEX_EOL;
                    _glPerspectiveDivideVertex(&tmp, h);
                    _glSubmitHeaderOrVertex(&tmp);
                } break;
                default:
                break;
            }

            /* If this was the last in the strip, we don't need to
            submit anything else, we just wipe the tri_count */
            if(is_last_in_strip) {
                tri_count = 0;
                strip_count = 0;
            } else {
                ShiftRotateTriangle();
                strip_count = 2;
            }
        } else {
            /* Invisible? Move to the next in the strip */

            if(is_last_in_strip) {
                tri_count = 0;
                strip_count = 0;
            }
            strip_count = 2;
            ShiftRotateTriangle();
        }
    }
}

void SceneListFinish() {
    uint32_t vidx = 0;
    const uint32_t* flags = (const uint32_t*) BUFFER;
    uint32_t step = sizeof(Vertex) / sizeof(uint32_t);

    for(int i = 0; i < vertex_counter; ++i, flags += step) {
        if((*flags & GPU_CMD_POLYHDR) == GPU_CMD_POLYHDR) {
            vidx = 0;

            uint32_t mode1 = *(flags + 1);
            // Extract culling mode
            uint32_t mask = mode1 & GPU_TA_PM1_CULLING_MASK;
            CULL_MODE = mask >> GPU_TA_PM1_CULLING_SHIFT;

        } else {
            switch(*flags) {
            case GPU_CMD_VERTEX_EOL:
            case GPU_CMD_VERTEX: // Fallthrough
                vidx++;
            break;
            default:
                break;
            }
        }

        if(vidx > 2) {
            Vertex* v0 = (Vertex*) (flags - step - step);
            Vertex* v1 = (Vertex*) (flags - step);
            Vertex* v2 = (Vertex*) (flags);
            (vidx % 2 == 0) ? DrawTriangle(v0, v1, v2) : DrawTriangle(v1, v0, v2);
        }

        if((*flags) == GPU_CMD_VERTEX_EOL) {
            vidx = 0;
        }
    }
}

void SceneFinish() {
    SDL_RenderPresent(RENDERER);
    return;
    /* Only sensible place to hook the quit signal */
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
              exit(0);
              break;
            default:
              break;
        }
    }
}

void UploadMatrix4x4(const Matrix4x4* mat) {
    memcpy(&MATRIX, mat, sizeof(Matrix4x4));
}

void MultiplyMatrix4x4(const Matrix4x4* mat) {
    Matrix4x4 product;

    product[0] = MATRIX[0] * (*mat)[0] + MATRIX[4] * (*mat)[1] + MATRIX[8] * (*mat)[2] + MATRIX[12] * (*mat)[3];
    product[1] = MATRIX[1] * (*mat)[0] + MATRIX[5] * (*mat)[1] + MATRIX[9] * (*mat)[2] + MATRIX[13] * (*mat)[3];
    product[2] = MATRIX[2] * (*mat)[0] + MATRIX[6] * (*mat)[1] + MATRIX[10] * (*mat)[2] + MATRIX[14] * (*mat)[3];
    product[3] = MATRIX[3] * (*mat)[0] + MATRIX[7] * (*mat)[1] + MATRIX[11] * (*mat)[2] + MATRIX[15] * (*mat)[3];

    product[4] = MATRIX[0] * (*mat)[4] + MATRIX[4] * (*mat)[5] + MATRIX[8] * (*mat)[6] + MATRIX[12] * (*mat)[7];
    product[5] = MATRIX[1] * (*mat)[4] + MATRIX[5] * (*mat)[5] + MATRIX[9] * (*mat)[6] + MATRIX[13] * (*mat)[7];
    product[6] = MATRIX[2] * (*mat)[4] + MATRIX[6] * (*mat)[5] + MATRIX[10] * (*mat)[6] + MATRIX[14] * (*mat)[7];
    product[7] = MATRIX[3] * (*mat)[4] + MATRIX[7] * (*mat)[5] + MATRIX[11] * (*mat)[6] + MATRIX[15] * (*mat)[7];

    product[8] = MATRIX[0] * (*mat)[8] + MATRIX[4] * (*mat)[9] + MATRIX[8] * (*mat)[10] + MATRIX[12] * (*mat)[11];
    product[9] = MATRIX[1] * (*mat)[8] + MATRIX[5] * (*mat)[9] + MATRIX[9] * (*mat)[10] + MATRIX[13] * (*mat)[11];
    product[10] = MATRIX[2] * (*mat)[8] + MATRIX[6] * (*mat)[9] + MATRIX[10] * (*mat)[10] + MATRIX[14] * (*mat)[11];
    product[11] = MATRIX[3] * (*mat)[8] + MATRIX[7] * (*mat)[9] + MATRIX[11] * (*mat)[10] + MATRIX[15] * (*mat)[11];

    product[12] = MATRIX[0] * (*mat)[12] + MATRIX[4] * (*mat)[13] + MATRIX[8] * (*mat)[14] + MATRIX[12] * (*mat)[15];
    product[13] = MATRIX[1] * (*mat)[12] + MATRIX[5] * (*mat)[13] + MATRIX[9] * (*mat)[14] + MATRIX[13] * (*mat)[15];
    product[14] = MATRIX[2] * (*mat)[12] + MATRIX[6] * (*mat)[13] + MATRIX[10] * (*mat)[14] + MATRIX[14] * (*mat)[15];
    product[15] = MATRIX[3] * (*mat)[12] + MATRIX[7] * (*mat)[13] + MATRIX[11] * (*mat)[14] + MATRIX[15] * (*mat)[15];

    UploadMatrix4x4(&product);
}

void DownloadMatrix4x4(Matrix4x4* mat) {
    memcpy(mat, &MATRIX, sizeof(Matrix4x4));
}

const VideoMode* GetVideoMode() {
    return &vid_mode;
}

size_t GPUMemoryAvailable() {
    return AVAILABLE_VRAM;
}

void* GPUMemoryAlloc(size_t size) {
    if(size > AVAILABLE_VRAM) {
        return NULL;
    } else {
        AVAILABLE_VRAM -= size;
        return malloc(size);
    }
}

void GPUSetPaletteFormat(GPUPaletteFormat format) {

}

void GPUSetPaletteEntry(uint32_t idx, uint32_t value) {

}

void GPUSetBackgroundColour(float r, float g, float b) {
    BACKGROUND_COLOR[0] = r * 255.0f;
    BACKGROUND_COLOR[1] = g * 255.0f;
    BACKGROUND_COLOR[2] = b * 255.0f;
}

void GPUSetAlphaCutOff(uint8_t v) {

}

void GPUSetClearDepth(float v) {

}

void GPUSetFogLinear(float start, float end) {

}

void GPUSetFogExp(float density) {

}

void GPUSetFogExp2(float density) {

}

void GPUSetFogColor(float r, float g, float b, float a) {

}

void TransformVec3NoMod(const float* v, float* ret) {
    ret[0] = v[0] * MATRIX[0] + v[1] * MATRIX[4] + v[2] * MATRIX[8] + 1.0f * MATRIX[12];
    ret[1] = v[0] * MATRIX[1] + v[1] * MATRIX[5] + v[2] * MATRIX[9] + 1.0f * MATRIX[13];
    ret[2] = v[0] * MATRIX[2] + v[1] * MATRIX[6] + v[2] * MATRIX[10] + 1.0f * MATRIX[14];
}

void TransformVec4NoMod(const float* v, float* ret) {
    ret[0] = v[0] * MATRIX[0] + v[1] * MATRIX[4] + v[2] * MATRIX[8] + v[3] * MATRIX[12];
    ret[1] = v[0] * MATRIX[1] + v[1] * MATRIX[5] + v[2] * MATRIX[9] + v[3] * MATRIX[13];
    ret[2] = v[0] * MATRIX[2] + v[1] * MATRIX[6] + v[2] * MATRIX[10] + v[3] * MATRIX[14];
    ret[3] = v[0] * MATRIX[3] + v[1] * MATRIX[7] + v[2] * MATRIX[11] + v[3] * MATRIX[15];
}

void TransformVec3(float* v) {
    float ret[3];
    TransformVec3NoMod(v, ret);
    FASTCPY(v, ret, sizeof(float) * 3);
}

void TransformVec4(float* v) {
    float ret[4];
    TransformVec4NoMod(v, ret);
    FASTCPY(v, ret, sizeof(float) * 4);
}

void TransformVertices(Vertex* vertices, const int count) {
    float ret[4];
    for(int i = 0; i < count; ++i, ++vertices) {
        ret[0] = vertices->xyz[0];
        ret[1] = vertices->xyz[1];
        ret[2] = vertices->xyz[2];
        ret[3] = 1.0f;

        TransformVec4(ret);

        vertices->xyz[0] = ret[0];
        vertices->xyz[1] = ret[1];
        vertices->xyz[2] = ret[2];
        vertices->w = ret[3];
    }
}

void TransformVertex(const float* xyz, const float* w, float* oxyz, float* ow) {
    float ret[4];
    ret[0] = xyz[0];
    ret[1] = xyz[1];
    ret[2] = xyz[2];
    ret[3] = *w;

    TransformVec4(ret);

    oxyz[0] = ret[0];
    oxyz[1] = ret[1];
    oxyz[2] = ret[2];
    *ow = ret[3];
}
