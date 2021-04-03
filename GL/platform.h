#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum GPUAlpha {
    GPU_ALPHA_DISABLE = 0,
    GPU_ALPHA_ENABLE = 1
} GPUAlpha;

typedef enum GPUTextureAlpha {
    GPU_TXRALPHA_DISABLE = 1,
    GPU_TXRALPHA_ENABLE = 0
} GPUTextureAlpha;

typedef enum GPUList {
    GPU_LIST_OP_POLY       = 0,
    GPU_LIST_OP_MOD        = 1,
    GPU_LIST_TR_POLY       = 2,
    GPU_LIST_TR_MOD        = 3,
    GPU_LIST_PT_POLY       = 4
} GPUList;

typedef enum GPUBlend {
    GPU_BLEND_ZERO          = 0,
    GPU_BLEND_ONE           = 1,
    GPU_BLEND_DESTCOLOR     = 2,
    GPU_BLEND_INVDESTCOLOR  = 3,
    GPU_BLEND_SRCALPHA      = 4,
    GPU_BLEND_INVSRCALPHA   = 5,
    GPU_BLEND_DESTALPHA     = 6,
    GPU_BLEND_INVDESTALPHA  = 7
} GPUBlend;


typedef enum GPUDepthCompare {
    GPU_DEPTHCMP_NEVER      = 0,
    GPU_DEPTHCMP_LESS       = 1,
    GPU_DEPTHCMP_EQUAL      = 2,
    GPU_DEPTHCMP_LEQUAL     = 3,
    GPU_DEPTHCMP_GREATER    = 4,
    GPU_DEPTHCMP_NOTEQUAL   = 5,
    GPU_DEPTHCMP_GEQUAL     = 6,
    GPU_DEPTHCMP_ALWAYS     = 7
} GPUDepthCompare;

/* Duplication of pvr_poly_cxt_t from KOS so that we can
 * compile on non-KOS platforms for testing */

typedef struct {
    GPUList     list_type;

    struct {
        int     alpha;
        int     shading;
        int     fog_type;
        int     culling;
        int     color_clamp;
        int     clip_mode;
        int     modifier_mode;
        int     specular;
        int     alpha2;
        int     fog_type2;
        int     color_clamp2;
    } gen;
    struct {
        int     src;
        int     dst;
        int     src_enable;
        int     dst_enable;
        int     src2;
        int     dst2;
        int     src_enable2;
        int     dst_enable2;
    } blend;
    struct {
        int     color;
        int     uv;
        int     modifier;
    } fmt;
    struct {
        int     comparison;
        int     write;
    } depth;
    struct {
        int     enable;
        int     filter;
        int     mipmap;
        int     mipmap_bias;
        int     uv_flip;
        int     uv_clamp;
        int     alpha;
        int     env;
        int     width;
        int     height;
        int     format;
        void*   base;
    } txr;
    struct {
        int     enable;
        int     filter;
        int     mipmap;
        int     mipmap_bias;
        int     uv_flip;
        int     uv_clamp;
        int     alpha;
        int     env;
        int     width;
        int     height;
        int     format;
        void*   base;
    } txr2;
} PolyContext;

typedef struct {
    uint32_t cmd;
    uint32_t mode1;
    uint32_t mode2;
    uint32_t mode3;
    uint32_t d1;
    uint32_t d2;
    uint32_t d3;
    uint32_t d4;
} PolyHeader;

enum GPUCommand {
    GPU_CMD_POLYHDR = 0x80840000,
    GPU_CMD_VERTEX = 0xe0000000,
    GPU_CMD_VERTEX_EOL = 0xf0000000,
    GPU_CMD_USERCLIP = 0x20000000,
    GPU_CMD_MODIFIER = 0x80000000,
    GPU_CMD_SPRITE = 0xA0000000
};

void SceneBegin();

void SceneListBegin(GPUList list);
void SceneListSubmit(void* src, int n);
void SceneListFinish();

void SceneFinish();



#ifdef __DREAMCAST__
#include "platforms/sh4.h"
#else
#include "platforms/x86.h"
#endif
