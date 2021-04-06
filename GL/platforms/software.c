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

static void DrawTriangle(const GPUVertex* v0, const GPUVertex* v1, const GPUVertex* v2) {
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

    ParameterEquation r, g, b;

    ParameterEquationInit(&r, v0->bgra[2], v1->bgra[2], v2->bgra[2], &e0, &e1, &e2, area);
    ParameterEquationInit(&g, v0->bgra[1], v1->bgra[1], v2->bgra[1], &e0, &e1, &e2, area);
    ParameterEquationInit(&b, v0->bgra[0], v1->bgra[0], v2->bgra[0], &e0, &e1, &e2, area);

    // Check if triangle is backfacing.

    if (area < 0) {
        return;
    }

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
    SDL_SetRenderDrawColor(RENDERER, 0, 0, 0, 0);
    SDL_RenderClear(RENDERER);
}

void SceneListBegin(GPUList list) {

}

void SceneListSubmit(void* src, int n) {
    uint32_t vertex_counter = 0;
    const uint32_t* flags = (const uint32_t*) src;
    uint32_t step = sizeof(GPUVertex) / sizeof(uint32_t);

    for(int i = 0; i < n; ++i, flags += step) {
        bool draw_triangle = false;
        switch(*flags) {
            case GPU_CMD_POLYHDR:
                break;
            case GPU_CMD_VERTEX_EOL:
                draw_triangle = (++vertex_counter == 3);
                vertex_counter = 0;
            break;
            case GPU_CMD_VERTEX: // Fallthrough
                vertex_counter++;
                draw_triangle = (vertex_counter == 3);
                if(draw_triangle) {
                    vertex_counter--;
                }
            break;
            default:
                break;
        }

        if(draw_triangle) {
            const GPUVertex* v0 = (const GPUVertex*) (flags - step - step);
            const GPUVertex* v1 = (const GPUVertex*) (flags - step);
            const GPUVertex* v2 = (const GPUVertex*) (flags);
            DrawTriangle(v0, v1, v2);
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

    for(int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            product[j + i * 4] = 0;
            for (int k = 0; k < 4; k++) {
                product[j + i * 4] += MATRIX[k + i * 4] * (*mat)[j + k * 4];
            }
        }
    }

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
