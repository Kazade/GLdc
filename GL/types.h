#pragma once

#include <stdint.h>

typedef uint16_t half_float_t;

/*
 *
 * Right this will take some explaining. This vertex matches one of the 64 byte vertex
 * structures that the PVR supports, with the following "adjustments" (aka, abuse of free bytes)

 * There are 8 unused bytes anyway, and we steal the 4 bytes of the offset color alpha channel
 * which we don't really need.

 * glSecondaryColor doesn't support an alpha channel, and it's unusual that you would need it
 * for the offset color. It's much more useful as a cache-friendly space for something else
 *
 * Normal is stored as X + Y, packed into half floats. Z is reconstructed (as normals should have a length of 1)
 */
typedef struct {
    uint32_t flags;
    float xyz[3];
    float uv[2];
    float w;
    uint32_t nxyz;
    float argb[4];
    half_float_t st[2];
    float offset_rgb[3];
} __attribute__ ((aligned (32))) Vertex;
