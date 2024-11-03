#pragma once

#include <stdint.h>

typedef struct {
    /* Same 32 byte layout as pvr_vertex_t */
    uint32_t flags;
    float xyz[3];
    float uv[2];
    uint8_t bgra[4];
    uint8_t oargb[4];
    // PVR vertex ends here
    float nxyz[3];    // 12
    float st[2];      // 20
    float w;          // 24
    float padding[2]; // 32
} __attribute__ ((aligned (32))) Vertex;
