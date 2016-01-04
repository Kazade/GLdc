/* KallistiGL for KallistiOS ##version##

   libgl/gl-clip.c
   Copyright (C) 2013-2014 Josh Pearson

   Near-Z Clipping Algorithm (C) 2013-2014 Josh PH3NOM Pearson
   Input Primitive Types Supported:
   -GL_TRIANGLES
   -GL_TRIANGLE_STRIPS
   -GL_QUADS
   Outputs a mix of Triangles and Triangle Strips for use with the PVR
*/

#include <stdio.h>
#include <string.h>

#include "gl.h"
#include "gl-api.h"
#include "gl-clip.h"

static float3 CLIP_BUF[1024 * 32];

static inline void _glKosVertexClipZNear(pvr_vertex_t *v1, pvr_vertex_t *v2, float MAG) {
    colorui *c1 = (colorui *)&v1->argb;
    colorui *c2 = (colorui *)&v2->argb;

    v1->x += (v2->x - v1->x) * MAG;
    v1->y += (v2->y - v1->y) * MAG;
    v1->z += (v2->z - v1->z) * MAG;
    v1->u += (v2->u - v1->u) * MAG;
    v1->v += (v2->v - v1->v) * MAG;
    c1->a += (c2->a - c1->a) * MAG;
    c1->r += (c2->r - c1->r) * MAG;
    c1->g += (c2->g - c1->g) * MAG;
    c1->b += (c2->b - c1->b) * MAG;
}

static GLuint _glKosTransformClip(pvr_vertex_t *v, GLint count) {
    pvr_vertex_t *V = v;
    float3 *C = CLIP_BUF;
    GLuint in = 0;

    while(count--) {
        mat_trans_single3_nodiv_nomod(V->x, V->y, V->z, C->x, C->y, C->z);

        if(C->z < CLIP_NEARZ) ++in;

        ++C;
        ++V;
    }

    return in;
}

GLuint _glKosClipTriangleStrip(pvr_vertex_t *src, pvr_vertex_t *dst, GLuint vertices) {
    GLuint i, v = 0, in = _glKosTransformClip(src, vertices);
    float3 *C = CLIP_BUF;

    if(in == vertices) {
        memcpy(dst, src, vertices * 0x20);
        pvr_vertex_t *v = dst;

        while(--in) {
            v->flags = PVR_CMD_VERTEX;
            ++v;
        }

        v->flags = PVR_CMD_VERTEX_EOL;

        return vertices;
    }
    else if(in == 0)
        return 0;

    /* Iterate all Triangles of the Strip - Hence looping vertices-2 times */
    for(i = 0; i < ((vertices) - 2); i++) {
        GLushort clip = 0; /* Clip Code for current Triangle */
        GLubyte verts_in = 0; /* # of Verteices inside clip plane for current Triangle */

        C->z     >= CLIP_NEARZ ? clip |= FIRST  : ++verts_in;
        (C + 1)->z >= CLIP_NEARZ ? clip |= SECOND : ++verts_in;
        (C + 2)->z >= CLIP_NEARZ ? clip |= THIRD  : ++verts_in;

        switch(verts_in) { /* Start by examining # of vertices inside clip plane */
            case 0: /* All Vertices of Triangle are outside of clip plne */
                break;

            case 3: /* All Vertices of Triangle are inside of clip plne */
                memcpy(&dst[v], &src[i], 96);

                dst[v].flags = dst[v + 1].flags = PVR_CMD_VERTEX;
                dst[v + 2].flags = PVR_CMD_VERTEX_EOL;

                v += 3;
                break;

            case 1:/* 1 Vertex of Triangle is inside of clip plane = output 1 Triangle */
                memcpy(&dst[v], &src[i], 96);

                switch(clip) {
                    case FIRST_TWO_OUT:
                        _glKosVertexClipZNear(&dst[v], &dst[v + 2], _glKosNearZClipMag(&CLIP_BUF[i], &CLIP_BUF[i + 2]));
                        _glKosVertexClipZNear(&dst[v + 1], &dst[v + 2], _glKosNearZClipMag(&CLIP_BUF[i + 1], &CLIP_BUF[i + 2]));

                        break;

                    case FIRST_AND_LAST_OUT:
                        _glKosVertexClipZNear(&dst[v], &dst[v + 1], _glKosNearZClipMag(&CLIP_BUF[i], &CLIP_BUF[i + 1]));
                        _glKosVertexClipZNear(&dst[v + 2], &dst[v + 1], _glKosNearZClipMag(&CLIP_BUF[i + 2], &CLIP_BUF[i + 1]));

                        break;

                    case LAST_TWO_OUT:
                        _glKosVertexClipZNear(&dst[v + 1], &dst[v], _glKosNearZClipMag(&CLIP_BUF[i + 1], &CLIP_BUF[i + 0]));
                        _glKosVertexClipZNear(&dst[v + 2], &dst[v], _glKosNearZClipMag(&CLIP_BUF[i + 2], &CLIP_BUF[i + 0]));

                        break;
                }

                dst[v].flags = dst[v + 1].flags = PVR_CMD_VERTEX;
                dst[v + 2].flags = PVR_CMD_VERTEX_EOL;
                v += 3;

                break;

            case 2:/* 2 Vertices of Triangle are inside of clip plane = output 2 Triangles */
                switch(clip) {
                    case FIRST:
                        _glKosVertexCopyPVR(&src[i + 0], &dst[v + 0]);
                        _glKosVertexCopyPVR(&src[i + 1], &dst[v + 1]);
                        _glKosVertexCopyPVR(&src[i + 0], &dst[v + 2]);
                        _glKosVertexCopyPVR(&src[i + 2], &dst[v + 3]);
                        _glKosVertexClipZNear(&dst[v + 0], &dst[v + 1], _glKosNearZClipMag(&CLIP_BUF[i], &CLIP_BUF[i + 1]));
                        _glKosVertexClipZNear(&dst[v + 2], &dst[v + 3], _glKosNearZClipMag(&CLIP_BUF[i], &CLIP_BUF[i + 2]));
                        break;

                    case SECOND:
                        _glKosVertexCopyPVR(&src[i + 1], &dst[v + 0]);
                        _glKosVertexCopyPVR(&src[i + 0], &dst[v + 1]);
                        _glKosVertexCopyPVR(&src[i + 1], &dst[v + 2]);
                        _glKosVertexCopyPVR(&src[i + 2], &dst[v + 3]);
                        _glKosVertexClipZNear(&dst[v + 0], &dst[v + 1], _glKosNearZClipMag(&CLIP_BUF[i + 1], &CLIP_BUF[i + 0]));
                        _glKosVertexClipZNear(&dst[v + 2], &dst[v + 3], _glKosNearZClipMag(&CLIP_BUF[i + 1], &CLIP_BUF[i + 2]));
                        break;

                    case THIRD:
                        _glKosVertexCopyPVR(&src[i + 2], &dst[v + 0]);
                        _glKosVertexCopyPVR(&src[i + 0], &dst[v + 1]);
                        _glKosVertexCopyPVR(&src[i + 2], &dst[v + 2]);
                        _glKosVertexCopyPVR(&src[i + 1], &dst[v + 3]);
                        _glKosVertexClipZNear(&dst[v + 0], &dst[v + 1], _glKosNearZClipMag(&CLIP_BUF[i + 2], &CLIP_BUF[i]));
                        _glKosVertexClipZNear(&dst[v + 2], &dst[v + 3], _glKosNearZClipMag(&CLIP_BUF[i + 2], &CLIP_BUF[i + 1]));
                        break;
                }

                dst[v + 0].flags = dst[v + 1].flags = dst[v + 2].flags = PVR_CMD_VERTEX;
                dst[v + 3].flags = PVR_CMD_VERTEX_EOL;
                v += 4;

                break;
        }

        C++;
    }

    return v;
}

static inline unsigned char _glKosClipTri(pvr_vertex_t *src, pvr_vertex_t *dst) {
    GLushort clip = 0; /* Clip Code for current Triangle */
    GLubyte verts_in = 0; /* # of Vertices inside clip plane for current Triangle */
    float3 clip_buf[3]; /* Store the Vertices for each Triangle Translated into Clip Space */

    /* Transform all 3 Vertices of Triangle */
    {
        register float __x __asm__("fr12") = src->x;
        register float __y __asm__("fr13") = src->y;
        register float __z __asm__("fr14") = src->z;

        mat_trans_fv12_nodiv();

        clip_buf[0].x = __x;
        clip_buf[0].y = __y;
        clip_buf[0].z = __z;

        __x = (src + 1)->x;
        __y = (src + 1)->y;
        __z = (src + 1)->z;

        mat_trans_fv12_nodiv();

        clip_buf[1].x = __x;
        clip_buf[1].y = __y;
        clip_buf[1].z = __z;

        __x = (src + 2)->x;
        __y = (src + 2)->y;
        __z = (src + 2)->z;

        mat_trans_fv12_nodiv();

        clip_buf[2].x = __x;
        clip_buf[2].y = __y;
        clip_buf[2].z = __z;

        /* Compute Clip Code for Triangle */
        (clip_buf[0].z >= CLIP_NEARZ) ? clip |= FIRST  : ++verts_in;
        (clip_buf[1].z >= CLIP_NEARZ) ? clip |= SECOND : ++verts_in;
        (clip_buf[2].z >= CLIP_NEARZ) ? clip |= THIRD  : ++verts_in;
    }

    switch(verts_in) { /* Start by examining # of vertices inside clip plane */
        case 0: /* All Vertices of Triangle are Outside of clip plne */
            return 0;

        case 3: /* All Vertices of Triangle are inside of clip plne */
            memcpy(dst, src, 96);

            dst->flags = (dst + 1)->flags = PVR_CMD_VERTEX;
            (dst + 2)->flags = PVR_CMD_VERTEX_EOL;

            return 3;

        case 1:/* 1 Vertex of Triangle is inside of clip plane = output 1 Triangle */
            memcpy(dst, src, 96);

            switch(clip) {
                case FIRST_TWO_OUT:
                    _glKosVertexClipZNear(dst, dst + 2, _glKosNearZClipMag(&clip_buf[0], &clip_buf[2]));
                    _glKosVertexClipZNear(dst + 1, dst + 2, _glKosNearZClipMag(&clip_buf[1], &clip_buf[2]));

                    break;

                case FIRST_AND_LAST_OUT:
                    _glKosVertexClipZNear(dst, dst + 1, _glKosNearZClipMag(&clip_buf[0], &clip_buf[1]));
                    _glKosVertexClipZNear(dst + 2, dst + 1, _glKosNearZClipMag(&clip_buf[2], &clip_buf[1]));

                    break;

                case LAST_TWO_OUT:
                    _glKosVertexClipZNear(dst + 1, dst, _glKosNearZClipMag(&clip_buf[1], &clip_buf[0]));
                    _glKosVertexClipZNear(dst + 2, dst, _glKosNearZClipMag(&clip_buf[2], &clip_buf[0]));

                    break;
            }

            dst->flags = (dst + 1)->flags = PVR_CMD_VERTEX;
            (dst + 2)->flags = PVR_CMD_VERTEX_EOL;

            return 3;

        case 2:/* 2 Vertices of Triangle are inside of clip plane = output 2 Triangles */
            switch(clip) {
                case FIRST:
                    memcpy(dst, src, 64);
                    _glKosVertexCopyPVR(src, dst + 2);
                    _glKosVertexCopyPVR(src + 2, dst + 3);
                    _glKosVertexClipZNear(dst, dst + 1, _glKosNearZClipMag(&clip_buf[0], &clip_buf[1]));
                    _glKosVertexClipZNear(dst + 2, dst + 3, _glKosNearZClipMag(&clip_buf[0], &clip_buf[2]));
                    break;

                case SECOND:
                    _glKosVertexCopyPVR(src + 1, dst);
                    _glKosVertexCopyPVR(src, dst + 1);
                    memcpy(dst + 2, src + 1, 64);
                    _glKosVertexClipZNear(dst, dst + 1, _glKosNearZClipMag(&clip_buf[1], &clip_buf[0]));
                    _glKosVertexClipZNear(dst + 2, dst + 3, _glKosNearZClipMag(&clip_buf[1], &clip_buf[2]));
                    break;

                case THIRD:
                    _glKosVertexCopyPVR(src + 2, dst);
                    _glKosVertexCopyPVR(src, dst + 1);
                    _glKosVertexCopyPVR(src + 2, dst + 2);
                    _glKosVertexCopyPVR(src + 1, dst + 3);
                    _glKosVertexClipZNear(dst, dst + 1, _glKosNearZClipMag(&clip_buf[2], &clip_buf[0]));
                    _glKosVertexClipZNear(dst + 2, dst + 3, _glKosNearZClipMag(&clip_buf[2], &clip_buf[1]));
                    break;
            }

            dst->flags = (dst + 1)->flags = (dst + 2)->flags = PVR_CMD_VERTEX;
            (dst + 3)->flags = PVR_CMD_VERTEX_EOL;

            return 4;
    }

    return 0;
}

GLuint _glKosClipTriangles(pvr_vertex_t *src, pvr_vertex_t *dst, GLuint vertices) {
    GLuint i, v = 0;

    for(i = 0; i < vertices; i += 3)   /* Iterate all Triangles */
        v += _glKosClipTri(src + i, dst + v);

    return v;
}

GLuint _glKosClipQuads(pvr_vertex_t *src, pvr_vertex_t *dst, GLuint vertices) {
    GLuint i, v = 0;
    pvr_vertex_t qv;

    for(i = 0; i < vertices; i += 4) { /* Iterate all Quads, Rearranging into Triangle Strips */
        _glKosVertexCopyPVR(src + i + 3, &qv);
        _glKosVertexCopyPVR(src + i + 2, src + i + 3);
        _glKosVertexCopyPVR(&qv, src + i + 2);
        v += _glKosClipTriangleStrip(src + i, dst + v, 4);
    }

    return v;
}
