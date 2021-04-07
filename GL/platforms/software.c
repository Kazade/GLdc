#include <SDL.h>

#include <stdlib.h>
#include <string.h>

#include "../platform.h"
#include "software.h"
#include "software/edge_equation.h"
#include "software/parameter_equation.h"

static size_t AVAILABLE_VRAM = 16 * 1024 * 1024;
static Matrix4x4 MATRIX;

static SDL_Window* WINDOW = NULL;
static SDL_Renderer* RENDERER = NULL;

static uint8_t BACKGROUND_COLOR[3] = {0, 0, 0};

GPUCulling CULL_MODE = GPU_CULLING_CCW;


static VideoMode vid_mode = {
    640, 480
};


typedef struct GPUVertex {
    uint32_t flags;
    float x;
    float y;
    float z;
    float u;
    float v;
    uint8_t bgra[4];
    uint8_t obgra[4];
} GPUVertex;

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

static void DrawTriangle(GPUVertex* v0, GPUVertex* v1, GPUVertex* v2) {
    // Compute triangle bounding box.

    int minX = MIN(MIN(v0->x, v1->x), v2->x);
    int maxX = MAX(MAX(v0->x, v1->x), v2->x);
    int minY = MIN(MIN(v0->y, v1->y), v2->y);
    int maxY = MAX(MAX(v0->y, v1->y), v2->y);

    // Clip to scissor rect.
/*
    minX = MAX(minX, m_minX);
    maxX = MIN(maxX, m_maxX);
    minY = MAX(minY, m_minY);
    maxY = MIN(maxY, m_maxY); */

    // Compute edge equations.

    EdgeEquation e0, e1, e2;
    EdgeEquationInit(&e0, &v0->x, &v1->x);
    EdgeEquationInit(&e1, &v1->x, &v2->x);
    EdgeEquationInit(&e2, &v2->x, &v0->x);

    float area = 0.5 * (e0.c + e1.c + e2.c);

    /* This is very ugly. I don't understand the math properly
     * so I just swap the vertex order if something is back-facing
     * and we want to render it. Patches welcome! */
#define REVERSE_WINDING() \
    GPUVertex* tv = v0; \
    v0 = v1; \
    v1 = tv; \
    EdgeEquationInit(&e0, &v0->x, &v1->x); \
    EdgeEquationInit(&e1, &v1->x, &v2->x); \
    EdgeEquationInit(&e2, &v2->x, &v0->x); \
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

void SceneListBegin(GPUList list) {

}

void SceneListSubmit(void* src, int n) {
    uint32_t vertex_counter = 0;
    const uint32_t* flags = (const uint32_t*) src;
    uint32_t step = sizeof(GPUVertex) / sizeof(uint32_t);

    for(int i = 0; i < n; ++i, flags += step) {
        if((*flags & GPU_CMD_POLYHDR) == GPU_CMD_POLYHDR) {
            vertex_counter = 0;

            uint32_t mode1 = *(flags + 1);
            // Extract culling mode
            uint32_t mask = mode1 & GPU_TA_PM1_CULLING_MASK;
            CULL_MODE = mask >> GPU_TA_PM1_CULLING_SHIFT;

        } else {
            switch(*flags) {
            case GPU_CMD_VERTEX_EOL:
            case GPU_CMD_VERTEX: // Fallthrough
                vertex_counter++;
            break;
            default:
                break;
            }
        }

        if(vertex_counter > 2) {
            GPUVertex* v0 = (GPUVertex*) (flags - step - step);
            GPUVertex* v1 = (GPUVertex*) (flags - step);
            GPUVertex* v2 = (GPUVertex*) (flags);
            (vertex_counter % 2 == 0) ? DrawTriangle(v0, v1, v2) : DrawTriangle(v1, v0, v2);
        }

        if((*flags) == GPU_CMD_VERTEX_EOL) {
            vertex_counter = 0;
        }
    }
}

void SceneListFinish() {

}

void SceneFinish() {
    SDL_RenderPresent(RENDERER);

    /* Only sensible place to hook the quit signal */

    SDL_Event e = {0};

    while (SDL_PollEvent(&e))
      switch (e.type) {
        case SDL_QUIT:
          exit(0);
          break;
        default:
          break;
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
