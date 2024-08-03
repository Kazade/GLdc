#include <SDL.h>

#include <stdlib.h>
#include <string.h>

#include "../private.h"
#include "../platform.h"
#include "software.h"
#include "software/edge_equation.h"
#include "software/parameter_equation.h"

#define CLIP_DEBUG 0
#define ZNEAR_CLIPPING_ENABLED 1

static size_t AVAILABLE_VRAM = 8 * 1024 * 1024;
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

AlignedVector vbuffer;

void InitGPU(_Bool autosort, _Bool fsaa) {

    // 32-bit SDL has trouble with the wayland driver for some reason
    setenv("SDL_VIDEODRIVER", "x11", 1);

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

    aligned_vector_init(&vbuffer, sizeof(SDL_Vertex));
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

GL_FORCE_INLINE void _glPerspectiveDivideVertex(Vertex* vertex) {
    const float f = 1.0f / (vertex->w);

    /* Convert to screenspace */
    /* (note that vertices have already been viewport transformed) */
    vertex->xyz[0] = vertex->xyz[0] * f;
    vertex->xyz[1] = vertex->xyz[1] * f;

    if(vertex->w == 1.0f) {
        vertex->xyz[2] = 1.0f / (1.0001f + vertex->xyz[2]);
    } else {
        vertex->xyz[2] = f;
    }
}

GL_FORCE_INLINE void _glPushHeaderOrVertex(const Vertex* v) {
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

static inline void _glFlushBuffer() {}


GL_FORCE_INLINE void _glClipEdge(const Vertex* v1, const Vertex* v2, Vertex* vout) {
    const static float o = 0.003921569f;  // 1 / 255
    const float d0 = v1->w + v1->xyz[2];
    const float d1 = v2->w + v2->xyz[2];
    const float t = (fabs(d0) * (1.0f / sqrtf((d1 - d0) * (d1 - d0)))) + 0.000001f;
    const float invt = 1.0f - t;

    vout->xyz[0] = invt * v1->xyz[0] + t * v2->xyz[0];
    vout->xyz[1] = invt * v1->xyz[1] + t * v2->xyz[1];
    vout->xyz[2] = invt * v1->xyz[2] + t * v2->xyz[2];

    vout->uv[0] = invt * v1->uv[0] + t * v2->uv[0];
    vout->uv[1] = invt * v1->uv[1] + t * v2->uv[1];

    vout->w = invt * v1->w + t * v2->w;

    const float m = 255 * t;
    const float n = 255 - m;

    vout->bgra[0] = (v1->bgra[0] * n + v2->bgra[0] * m) * o;
    vout->bgra[1] = (v1->bgra[1] * n + v2->bgra[1] * m) * o;
    vout->bgra[2] = (v1->bgra[2] * n + v2->bgra[2] * m) * o;
    vout->bgra[3] = (v1->bgra[3] * n + v2->bgra[3] * m) * o;
}

void SceneListSubmit(Vertex* v2, int n) {
    /* You need at least a header, and 3 vertices to render anything */
    if(n < 4) {
        return;
    }

    uint8_t visible_mask = 0;
    uint8_t counter = 0;

    for(int i = 0; i < n; ++i, ++v2) {
        PREFETCH(v2 + 1);
        switch(v2->flags) {
            case GPU_CMD_VERTEX_EOL:
                if(counter < 2) {
                    continue;
                }
                counter = 0;
            break;
            case GPU_CMD_VERTEX:
                ++counter;
                if(counter < 3) {
                    continue;
                }
            break;
            default:
                _glPushHeaderOrVertex(v2);
                counter = 0;
                continue;
        };

        Vertex* const v0 = v2 - 2;
        Vertex* const v1 = v2 - 1;

        visible_mask = (
            (v0->xyz[2] > -v0->w) << 0 |
            (v1->xyz[2] > -v1->w) << 1 |
            (v2->xyz[2] > -v2->w) << 2 |
            (counter == 0) << 3
        );

        switch(visible_mask) {
        case 15: /* All visible, but final vertex in strip */
        {
            _glPerspectiveDivideVertex(v0);
            _glPushHeaderOrVertex(v0);

            _glPerspectiveDivideVertex(v1);
            _glPushHeaderOrVertex(v1);

            _glPerspectiveDivideVertex(v2);
            _glPushHeaderOrVertex(v2);
        }
        break;
        case 7:
            /* All visible, push the first vertex and move on */
            _glPerspectiveDivideVertex(v0);
            _glPushHeaderOrVertex(v0);
        break;
        case 9:
      /* First vertex was visible, last in strip */
        {
            Vertex __attribute__((aligned(32))) scratch[2];
            Vertex* a = &scratch[0];
            Vertex* b = &scratch[1];

            _glClipEdge(v0, v1, a);
            a->flags = GPU_CMD_VERTEX;

            _glClipEdge(v2, v0, b);
            b->flags = GPU_CMD_VERTEX_EOL;

            _glPerspectiveDivideVertex(v0);
            _glPushHeaderOrVertex(v0);

            _glPerspectiveDivideVertex(a);
            _glPushHeaderOrVertex(a);

            _glPerspectiveDivideVertex(b);
            _glPushHeaderOrVertex(b);
        }
        break;
        case 1:
        /* First vertex was visible, but not last in strip */
        {
            Vertex __attribute__((aligned(32))) scratch[2];
            Vertex* a = &scratch[0];
            Vertex* b = &scratch[1];

            _glClipEdge(v0, v1, a);
            a->flags = GPU_CMD_VERTEX;

            _glClipEdge(v2, v0, b);
            b->flags = GPU_CMD_VERTEX;

            _glPerspectiveDivideVertex(v0);
            _glPushHeaderOrVertex(v0);

            _glPerspectiveDivideVertex(a);
            _glPushHeaderOrVertex(a);

            _glPerspectiveDivideVertex(b);
            _glPushHeaderOrVertex(b);
            _glPushHeaderOrVertex(b);
        }
        break;
        case 10:
        case 2:
        /* Second vertex was visible. In self case we need to create a triangle and produce
                two new vertices: 1-2, and 2-3. */
        {
            Vertex __attribute__((aligned(32))) scratch[3];
            Vertex* a = &scratch[0];
            Vertex* b = &scratch[1];
            Vertex* c = &scratch[2];

            memcpy_vertex(c, v1);

            _glClipEdge(v0, v1, a);
            a->flags = GPU_CMD_VERTEX;

            _glClipEdge(v1, v2, b);
            b->flags = v2->flags;

            _glPerspectiveDivideVertex(a);
            _glPushHeaderOrVertex(a);

            _glPerspectiveDivideVertex(c);
            _glPushHeaderOrVertex(c);

            _glPerspectiveDivideVertex(b);
            _glPushHeaderOrVertex(b);
        }
        break;
        case 11:
        case 3:  /* First and second vertex were visible */
        {
            Vertex __attribute__((aligned(32))) scratch[3];
            Vertex* a = &scratch[0];
            Vertex* b = &scratch[1];
            Vertex* c = &scratch[2];

            memcpy_vertex(c, v1);

            _glClipEdge(v2, v0, b);
            b->flags = GPU_CMD_VERTEX;

            _glPerspectiveDivideVertex(v0);
            _glPushHeaderOrVertex(v0);

            _glClipEdge(v1, v2, a);
            a->flags = v2->flags;

            _glPerspectiveDivideVertex(c);
            _glPushHeaderOrVertex(c);

            _glPerspectiveDivideVertex(b);
            _glPushHeaderOrVertex(b);

            _glPerspectiveDivideVertex(a);
            _glPushHeaderOrVertex(c);
            _glPushHeaderOrVertex(a);
        }
        break;
        case 12:
        case 4:
        /* Third vertex was visible. */
        {
            Vertex __attribute__((aligned(32))) scratch[3];
            Vertex* a = &scratch[0];
            Vertex* b = &scratch[1];
            Vertex* c = &scratch[2];

            memcpy_vertex(c, v2);

            _glClipEdge(v2, v0, a);
            a->flags = GPU_CMD_VERTEX;

            _glClipEdge(v1, v2, b);
            b->flags = GPU_CMD_VERTEX;

            _glPerspectiveDivideVertex(a);
            _glPushHeaderOrVertex(a);

            if(counter % 2 == 1) {
                _glPushHeaderOrVertex(a);
            }

            _glPerspectiveDivideVertex(b);
            _glPushHeaderOrVertex(b);

            _glPerspectiveDivideVertex(c);
            _glPushHeaderOrVertex(c);
        }
        break;
        case 13:
        {
            Vertex __attribute__((aligned(32))) scratch[3];
            Vertex* a = &scratch[0];
            Vertex* b = &scratch[1];
            Vertex* c = &scratch[2];

            memcpy_vertex(c, v2);
            c->flags = GPU_CMD_VERTEX;

            _glClipEdge(v0, v1, a);
            a->flags = GPU_CMD_VERTEX;

            _glClipEdge(v1, v2, b);
            b->flags = GPU_CMD_VERTEX;

            _glPerspectiveDivideVertex(v0);
            _glPushHeaderOrVertex(v0);

            _glPerspectiveDivideVertex(a);
            _glPushHeaderOrVertex(a);

            _glPerspectiveDivideVertex(c);
            _glPushHeaderOrVertex(c);
            _glPerspectiveDivideVertex(b);
            _glPushHeaderOrVertex(b);

            c->flags = GPU_CMD_VERTEX_EOL;
            _glPushHeaderOrVertex(c);
        }
        break;
        case 5:  /* First and third vertex were visible */
        {
            Vertex __attribute__((aligned(32))) scratch[3];
            Vertex* a = &scratch[0];
            Vertex* b = &scratch[1];
            Vertex* c = &scratch[2];

            memcpy_vertex(c, v2);
            c->flags = GPU_CMD_VERTEX;

            _glClipEdge(v0, v1, a);
            a->flags = GPU_CMD_VERTEX;

            _glClipEdge(v1, v2, b);
            b->flags = GPU_CMD_VERTEX;

            _glPerspectiveDivideVertex(v0);
            _glPushHeaderOrVertex(v0);

            _glPerspectiveDivideVertex(a);
            _glPushHeaderOrVertex(a);

            _glPerspectiveDivideVertex(c);
            _glPushHeaderOrVertex(c);
            _glPerspectiveDivideVertex(b);
            _glPushHeaderOrVertex(b);
            _glPushHeaderOrVertex(c);
        }
        break;
        case 14:
        case 6:  /* Second and third vertex were visible */
        {
            Vertex __attribute__((aligned(32))) scratch[4];
            Vertex* a = &scratch[0];
            Vertex* b = &scratch[1];
            Vertex* c = &scratch[2];
            Vertex* d = &scratch[3];

            memcpy_vertex(c, v1);
            memcpy_vertex(d, v2);

            _glClipEdge(v0, v1, a);
            a->flags = GPU_CMD_VERTEX;

            _glClipEdge(v2, v0, b);
            b->flags = GPU_CMD_VERTEX;

            _glPerspectiveDivideVertex(a);
            _glPushHeaderOrVertex(a);

            _glPerspectiveDivideVertex(c);
            _glPushHeaderOrVertex(c);

            _glPerspectiveDivideVertex(b);
            _glPushHeaderOrVertex(b);
            _glPushHeaderOrVertex(c);

            _glPerspectiveDivideVertex(d);
            _glPushHeaderOrVertex(d);
        }
        break;
        case 8:
        default:
        break;
        }
    }

    _glFlushBuffer();
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

            SDL_Vertex sv0 = {
                {v0->xyz[0], v0->xyz[1]},
                {v0->bgra[2], v0->bgra[1], v0->bgra[0], v0->bgra[3]},
                {v0->uv[0], v0->uv[1]}
            };

            SDL_Vertex sv1 = {
                {v1->xyz[0], v1->xyz[1]},
                {v1->bgra[2], v1->bgra[1], v1->bgra[0], v1->bgra[3]},
                {v1->uv[0], v1->uv[1]}
            };

            SDL_Vertex sv2 = {
                {v2->xyz[0], v2->xyz[1]},
                {v2->bgra[2], v2->bgra[1], v2->bgra[0], v2->bgra[3]},
                {v2->uv[0], v2->uv[1]}
            };

            aligned_vector_push_back(&vbuffer, &sv0, 1);
            aligned_vector_push_back(&vbuffer, &sv1, 1);
            aligned_vector_push_back(&vbuffer, &sv2, 1);
        }

        if((*flags) == GPU_CMD_VERTEX_EOL) {
            vidx = 0;
        }
    }

    SDL_SetRenderDrawColor(RENDERER, 255, 255, 255, 255);
    SDL_RenderGeometry(RENDERER, NULL, aligned_vector_front(&vbuffer), aligned_vector_size(&vbuffer), NULL, 0);
}

void SceneFinish() {
    SDL_RenderPresent(RENDERER);
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
