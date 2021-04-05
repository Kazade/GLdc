#include <stdlib.h>
#include <string.h>

#include "../platform.h"
#include "x86.h"

static size_t AVAILABLE_VRAM = 16 * 1024 * 1024;
static Matrix4x4 MATRIX;

void InitGPU(_Bool autosort, _Bool fsaa) {

}

void SceneBegin() {

}

void SceneListBegin(GPUList list) {

}

void SceneListSubmit(void* src, int n) {

}

void SceneListFinish() {

}

void SceneFinish() {

}

void UploadMatrix4x4(const Matrix4x4* mat) {
    memcpy(&MATRIX, mat, sizeof(Matrix4x4));
}

void MultiplyMatrix4x4(const Matrix4x4* mat) {
    (void) mat;
}

void DownloadMatrix4x4(Matrix4x4* mat) {
    memcpy(mat, &MATRIX, sizeof(Matrix4x4));
}

static VideoMode vid_mode = {
    640, 480
};

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
