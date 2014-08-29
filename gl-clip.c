/* KallistiGL for KallistiOS ##version##

   libgl/gl-clip.c
   Copyright (C) 2013-2014 Josh "PH3NOM" Pearson

   Near-Z Clipping Algorithm (C) 2013-2014 Josh PH3NOM Pearson
   Input Primitive Types Supported:
   -GL_TRIANGLES
   -GL_TRIANGLE_STRIPS
   -GL_QUADS
   Outputs a mix of Triangles and Triangle Strips for use with the PVR
*/

#include <stdio.h>
#include <string.h>

#include "gl-clip.h"

static float3 CLIP_BUF[1024 * 32];

static inline void glVertexCopy3f(float3 *src, float3 *dst) {
    *dst = *src;
}

static inline void glVertexCopyPVR(pvr_vertex_t *src, pvr_vertex_t *dst) {
    *dst = *src;
}

static inline void _glKosVertexCopyPVR(pvr_vertex_t *src, pvr_vertex_t *dst) {
    *dst = *src;
}

static inline float ZclipMag(float3 *v1, float3 *v2) {
    return ((CLIP_NEARZ - v1->z) / (v2->z - v1->z));
}

static inline void glVertexClipZNear(pvr_vertex_t *v1, pvr_vertex_t *v2, float MAG) {
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

static inline void _glKosVertexClipZNear3(pvr_vertex_t *v1, pvr_vertex_t *v2, float *w1, float *w2) {
    float MAG = ((CLIP_NEARZ - v1->z) / (v2->z - v1->z));

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

    *w1 += (*w2 - *w1) * MAG;
}

static GLuint glTransformClip(pvr_vertex_t *v, int count) {
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

GLuint _glKosClipTriangleStrip(pvr_vertex_t *vin, pvr_vertex_t *vout, unsigned int vertices) {
    GLuint i, v = 0, in = glTransformClip(vin, vertices);
    float3 *C = CLIP_BUF;

    if(in == vertices) {
        memcpy(vout, vin, vertices * 0x20);
        pvr_vertex_t *v = vout;

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
        unsigned short int clip = 0; /* Clip Code for current Triangle */
        unsigned char verts_in = 0; /* # of Verteices inside clip plane for current Triangle */

        C->z     >= CLIP_NEARZ ? clip |= FIRST  : ++verts_in;
        (C + 1)->z >= CLIP_NEARZ ? clip |= SECOND : ++verts_in;
        (C + 2)->z >= CLIP_NEARZ ? clip |= THIRD  : ++verts_in;

        switch(verts_in) { /* Start by examining # of vertices inside clip plane */
            case 0: /* All Vertices of Triangle are outside of clip plne */
                break;

            case 3: /* All Vertices of Triangle are inside of clip plne */
                memcpy(&vout[v], &vin[i], 96);

                vout[v].flags = vout[v + 1].flags = PVR_CMD_VERTEX;
                vout[v + 2].flags = PVR_CMD_VERTEX_EOL;

                v += 3;
                break;

            case 1:/* 1 Vertex of Triangle is inside of clip plane = output 1 Triangle */
                memcpy(&vout[v], &vin[i], 96);

                switch(clip) {
                    case FIRST_TWO_OUT:
                        glVertexClipZNear(&vout[v], &vout[v + 2], ZclipMag(&CLIP_BUF[i], &CLIP_BUF[i + 2]));
                        glVertexClipZNear(&vout[v + 1], &vout[v + 2], ZclipMag(&CLIP_BUF[i + 1], &CLIP_BUF[i + 2]));

                        break;

                    case FIRST_AND_LAST_OUT:
                        glVertexClipZNear(&vout[v], &vout[v + 1], ZclipMag(&CLIP_BUF[i], &CLIP_BUF[i + 1]));
                        glVertexClipZNear(&vout[v + 2], &vout[v + 1], ZclipMag(&CLIP_BUF[i + 2], &CLIP_BUF[i + 1]));

                        break;

                    case LAST_TWO_OUT:
                        glVertexClipZNear(&vout[v + 1], &vout[v], ZclipMag(&CLIP_BUF[i + 1], &CLIP_BUF[i + 0]));
                        glVertexClipZNear(&vout[v + 2], &vout[v], ZclipMag(&CLIP_BUF[i + 2], &CLIP_BUF[i + 0]));

                        break;
                }

                vout[v].flags = vout[v + 1].flags = PVR_CMD_VERTEX;
                vout[v + 2].flags = PVR_CMD_VERTEX_EOL;
                v += 3;

                break;

            case 2:/* 2 Vertices of Triangle are inside of clip plane = output 2 Triangles */
                switch(clip) {
                    case FIRST:
                        glVertexCopyPVR(&vin[i + 0], &vout[v + 0]);
                        glVertexCopyPVR(&vin[i + 1], &vout[v + 1]);
                        glVertexCopyPVR(&vin[i + 0], &vout[v + 2]);
                        glVertexCopyPVR(&vin[i + 2], &vout[v + 3]);
                        glVertexClipZNear(&vout[v + 0], &vout[v + 1], ZclipMag(&CLIP_BUF[i], &CLIP_BUF[i + 1]));
                        glVertexClipZNear(&vout[v + 2], &vout[v + 3], ZclipMag(&CLIP_BUF[i], &CLIP_BUF[i + 2]));
                        break;

                    case SECOND:
                        glVertexCopyPVR(&vin[i + 1], &vout[v + 0]);
                        glVertexCopyPVR(&vin[i + 0], &vout[v + 1]);
                        glVertexCopyPVR(&vin[i + 1], &vout[v + 2]);
                        glVertexCopyPVR(&vin[i + 2], &vout[v + 3]);
                        glVertexClipZNear(&vout[v + 0], &vout[v + 1], ZclipMag(&CLIP_BUF[i + 1], &CLIP_BUF[i + 0]));
                        glVertexClipZNear(&vout[v + 2], &vout[v + 3], ZclipMag(&CLIP_BUF[i + 1], &CLIP_BUF[i + 2]));
                        break;

                    case THIRD:
                        glVertexCopyPVR(&vin[i + 2], &vout[v + 0]);
                        glVertexCopyPVR(&vin[i + 0], &vout[v + 1]);
                        glVertexCopyPVR(&vin[i + 2], &vout[v + 2]);
                        glVertexCopyPVR(&vin[i + 1], &vout[v + 3]);
                        glVertexClipZNear(&vout[v + 0], &vout[v + 1], ZclipMag(&CLIP_BUF[i + 2], &CLIP_BUF[i]));
                        glVertexClipZNear(&vout[v + 2], &vout[v + 3], ZclipMag(&CLIP_BUF[i + 2], &CLIP_BUF[i + 1]));
                        break;
                }

                vout[v + 0].flags = vout[v + 1].flags = vout[v + 2].flags = PVR_CMD_VERTEX;
                vout[v + 3].flags = PVR_CMD_VERTEX_EOL;
                v += 4;

                break;
        }

        C++;
    }

    return v;
}

static inline unsigned char _glKosClipTri(pvr_vertex_t *vin, pvr_vertex_t *vout) {
    unsigned short int clip = 0; /* Clip Code for current Triangle */
    unsigned char verts_in = 0; /* # of Vertices inside clip plane for current Triangle */
    float3 clip_buf[3]; /* Store the Vertices for each Triangle Translated into Clip Space */

    /* Transform all 3 Vertices of Triangle */
    {
        register float __x __asm__("fr12") = vin->x;
        register float __y __asm__("fr13") = vin->y;
        register float __z __asm__("fr14") = vin->z;

        mat_trans_fv12_nodiv();

        clip_buf[0].x = __x;
        clip_buf[0].y = __y;
        clip_buf[0].z = __z;

        __x = (vin + 1)->x;
        __y = (vin + 1)->y;
        __z = (vin + 1)->z;

        mat_trans_fv12_nodiv();

        clip_buf[1].x = __x;
        clip_buf[1].y = __y;
        clip_buf[1].z = __z;

        __x = (vin + 2)->x;
        __y = (vin + 2)->y;
        __z = (vin + 2)->z;

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
            memcpy(vout, vin, 96);

            vout->flags = (vout + 1)->flags = PVR_CMD_VERTEX;
            (vout + 2)->flags = PVR_CMD_VERTEX_EOL;

            return 3;

        case 1:/* 1 Vertex of Triangle is inside of clip plane = output 1 Triangle */
            memcpy(vout, vin, 96);

            switch(clip) {
                case FIRST_TWO_OUT:
                    glVertexClipZNear(vout, vout + 2, ZclipMag(&clip_buf[0], &clip_buf[2]));
                    glVertexClipZNear(vout + 1, vout + 2, ZclipMag(&clip_buf[1], &clip_buf[2]));

                    break;

                case FIRST_AND_LAST_OUT:
                    glVertexClipZNear(vout, vout + 1, ZclipMag(&clip_buf[0], &clip_buf[1]));
                    glVertexClipZNear(vout + 2, vout + 1, ZclipMag(&clip_buf[2], &clip_buf[1]));

                    break;

                case LAST_TWO_OUT:
                    glVertexClipZNear(vout + 1, vout, ZclipMag(&clip_buf[1], &clip_buf[0]));
                    glVertexClipZNear(vout + 2, vout, ZclipMag(&clip_buf[2], &clip_buf[0]));

                    break;
            }

            vout->flags = (vout + 1)->flags = PVR_CMD_VERTEX;
            (vout + 2)->flags = PVR_CMD_VERTEX_EOL;

            return 3;

        case 2:/* 2 Vertices of Triangle are inside of clip plane = output 2 Triangles */
            switch(clip) {
                case FIRST:
                    memcpy(vout, vin, 64);
                    glVertexCopyPVR(vin, vout + 2);
                    glVertexCopyPVR(vin + 2, vout + 3);
                    glVertexClipZNear(vout, vout + 1, ZclipMag(&clip_buf[0], &clip_buf[1]));
                    glVertexClipZNear(vout + 2, vout + 3, ZclipMag(&clip_buf[0], &clip_buf[2]));
                    break;

                case SECOND:
                    glVertexCopyPVR(vin + 1, vout);
                    glVertexCopyPVR(vin, vout + 1);
                    memcpy(vout + 2, vin + 1, 64);
                    glVertexClipZNear(vout, vout + 1, ZclipMag(&clip_buf[1], &clip_buf[0]));
                    glVertexClipZNear(vout + 2, vout + 3, ZclipMag(&clip_buf[1], &clip_buf[2]));
                    break;

                case THIRD:
                    glVertexCopyPVR(vin + 2, vout);
                    glVertexCopyPVR(vin, vout + 1);
                    glVertexCopyPVR(vin + 2, vout + 2);
                    glVertexCopyPVR(vin + 1, vout + 3);
                    glVertexClipZNear(vout, vout + 1, ZclipMag(&clip_buf[2], &clip_buf[0]));
                    glVertexClipZNear(vout + 2, vout + 3, ZclipMag(&clip_buf[2], &clip_buf[1]));
                    break;
            }

            vout->flags = (vout + 1)->flags = (vout + 2)->flags = PVR_CMD_VERTEX;
            (vout + 3)->flags = PVR_CMD_VERTEX_EOL;

            return 4;
    }

    return 0;
}

unsigned int _glKosClipTriangles(pvr_vertex_t *vin, pvr_vertex_t *vout, unsigned int vertices) {
    unsigned int i, v = 0;

    for(i = 0; i < vertices; i += 3)   /* Iterate all Triangles */
        v += _glKosClipTri(vin + i, vout + v);

    return v;
}

unsigned int _glKosClipQuads(pvr_vertex_t *vin, pvr_vertex_t *vout, unsigned int vertices) {
    unsigned int i, v = 0;
    pvr_vertex_t qv;

    for(i = 0; i < vertices; i += 4) { /* Iterate all Quads, Rearranging into Triangle Strips */
        glVertexCopyPVR(vin + i + 3, &qv);
        glVertexCopyPVR(vin + i + 2, vin + i + 3);
        glVertexCopyPVR(&qv, vin + i + 2);
        v += _glKosClipTriangleStrip(vin + i, vout + v, 4);
    }

    return v;
}

static inline void _glKosVertexPerspectiveDivide(pvr_vertex_t *dst, float w) {
    dst->z = 1.0f / w;
    dst->x *= dst->z;
    dst->y *= dst->z;
}

GLuint _glKosClipTrianglesTransformed(pvr_vertex_t *src, float *w, pvr_vertex_t *dst, GLuint count) {
    pvr_vertex_t *vin = src;
    pvr_vertex_t *vout = dst;
    unsigned short int clip;
    unsigned char verts_in = 0;
    GLuint verts_out = 0;
    GLuint i;

    for(i = 0; i < count; i += 3) {
        clip = 0; /* Clip Code for current Triangle */
        verts_in = 0; /* # of Vertices inside clip plane for current Triangle */
        float W[4] = { w[i], w[i + 1], w[i + 2] }; /* W Component for Perspective Divide */

        (vin[0].z >= CLIP_NEARZ) ? clip |= FIRST  : ++verts_in;
        (vin[1].z >= CLIP_NEARZ) ? clip |= SECOND : ++verts_in;
        (vin[2].z >= CLIP_NEARZ) ? clip |= THIRD  : ++verts_in;

        switch(verts_in) { /* Start by examining # of vertices inside clip plane */
            case 0: /* All Vertices of Triangle are Outside of clip plne */
                break;

            case 3: /* All Vertices of Triangle are inside of clip plne */
                _glKosVertexCopyPVR(&vin[0], &vout[0]);
                _glKosVertexCopyPVR(&vin[1], &vout[1]);
                _glKosVertexCopyPVR(&vin[2], &vout[2]);

                _glKosVertexPerspectiveDivide(&vout[0], W[0]);
                _glKosVertexPerspectiveDivide(&vout[1], W[1]);
                _glKosVertexPerspectiveDivide(&vout[2], W[2]);

                vout[0].flags = vout[1].flags = PVR_CMD_VERTEX;
                vout[2].flags = PVR_CMD_VERTEX_EOL;

                vout += 3;
                verts_out += 3;
                break;

            case 1:/* 1 Vertex of Triangle is inside of clip plane = output 1 Triangle */
                _glKosVertexCopyPVR(&vin[0], &vout[0]);
                _glKosVertexCopyPVR(&vin[1], &vout[1]);
                _glKosVertexCopyPVR(&vin[2], &vout[2]);

                switch(clip) {
                    case FIRST_TWO_OUT:
                        _glKosVertexClipZNear3(&vout[0], &vout[2], &W[0], &W[2]);
                        _glKosVertexClipZNear3(&vout[1], &vout[2], &W[1], &W[2]);

                        break;

                    case FIRST_AND_LAST_OUT:
                        _glKosVertexClipZNear3(&vout[0], &vout[1], &W[0], &W[1]);
                        _glKosVertexClipZNear3(&vout[2], &vout[1], &W[2], &W[1]);

                        break;

                    case LAST_TWO_OUT:
                        _glKosVertexClipZNear3(&vout[1], &vout[0], &W[1], &W[0]);
                        _glKosVertexClipZNear3(&vout[2], &vout[0], &W[2], &W[0]);

                        break;
                }

                _glKosVertexPerspectiveDivide(&vout[0], W[0]);
                _glKosVertexPerspectiveDivide(&vout[1], W[1]);
                _glKosVertexPerspectiveDivide(&vout[2], W[2]);

                vout[0].flags = vout[1].flags = PVR_CMD_VERTEX;
                vout[2].flags = PVR_CMD_VERTEX_EOL;

                vout += 3;
                verts_out += 3;
                break;

            case 2:/* 2 Vertices of Triangle are inside of clip plane = output 2 Triangles */
                switch(clip) {
                    case FIRST:
                        _glKosVertexCopyPVR(&vin[0], &vout[0]);
                        _glKosVertexCopyPVR(&vin[1], &vout[1]);
                        _glKosVertexCopyPVR(&vin[0], &vout[2]);
                        _glKosVertexCopyPVR(&vin[2], &vout[3]);
                        W[3] = W[2];
                        W[2] = W[0];

                        break;

                    case SECOND:
                        _glKosVertexCopyPVR(&vin[1], &vout[0]);
                        _glKosVertexCopyPVR(&vin[2], &vout[1]);
                        _glKosVertexCopyPVR(&vin[1], &vout[2]);
                        _glKosVertexCopyPVR(&vin[0], &vout[3]);
                        W[3] = W[0];
                        W[0] = W[1];
                        W[1] = W[2];
                        W[2] = W[0];

                        break;

                    case THIRD:
                        _glKosVertexCopyPVR(&vin[2], &vout[0]);
                        _glKosVertexCopyPVR(&vin[0], &vout[1]);
                        _glKosVertexCopyPVR(&vin[2], &vout[2]);
                        _glKosVertexCopyPVR(&vin[1], &vout[3]);
                        W[3] = W[1];
                        W[1] = W[0];
                        W[0] = W[2];

                        break;
                }

                _glKosVertexClipZNear3(&vout[0], &vout[1], &W[0], &W[1]);
                _glKosVertexClipZNear3(&vout[2], &vout[3], &W[2], &W[3]);

                _glKosVertexPerspectiveDivide(&vout[0], W[0]);
                _glKosVertexPerspectiveDivide(&vout[1], W[1]);
                _glKosVertexPerspectiveDivide(&vout[2], W[2]);
                _glKosVertexPerspectiveDivide(&vout[3], W[3]);

                vout[0].flags = vout[1].flags = vout[2].flags = PVR_CMD_VERTEX;
                vout[3].flags = PVR_CMD_VERTEX_EOL;

                vout += 4;
                verts_out += 4;
                break;
        }

        vin += 3;
    }

    return verts_out;
}

GLuint _glKosClipTriangleStripTransformed(pvr_vertex_t *src, float *w, pvr_vertex_t *dst, GLuint count) {
    pvr_vertex_t *vin = src;
    pvr_vertex_t *vout = dst;
    unsigned short int clip;
    unsigned char verts_in = 0;
    GLuint verts_out = 0;
    GLuint i;

    for(i = 0; i < (count - 2); i ++) {
        clip = 0; /* Clip Code for current Triangle */
        verts_in = 0; /* # of Vertices inside clip plane for current Triangle */
        float W[4] = { w[i], w[i + 1], w[i + 2] }; /* W Component for Perspective Divide */

        (vin[0].z >= CLIP_NEARZ) ? clip |= FIRST  : ++verts_in;
        (vin[1].z >= CLIP_NEARZ) ? clip |= SECOND : ++verts_in;
        (vin[2].z >= CLIP_NEARZ) ? clip |= THIRD  : ++verts_in;

        switch(verts_in) { /* Start by examining # of vertices inside clip plane */
            case 0: /* All Vertices of Triangle are Outside of clip plne */
                break;

            case 3: /* All Vertices of Triangle are inside of clip plne */
                _glKosVertexCopyPVR(&vin[0], &vout[0]);
                _glKosVertexCopyPVR(&vin[1], &vout[1]);
                _glKosVertexCopyPVR(&vin[2], &vout[2]);

                _glKosVertexPerspectiveDivide(&vout[0], W[0]);
                _glKosVertexPerspectiveDivide(&vout[1], W[1]);
                _glKosVertexPerspectiveDivide(&vout[2], W[2]);

                vout[0].flags = vout[1].flags = PVR_CMD_VERTEX;
                vout[2].flags = PVR_CMD_VERTEX_EOL;

                vout += 3;
                verts_out += 3;
                break;

            case 1:/* 1 Vertex of Triangle is inside of clip plane = output 1 Triangle */
                _glKosVertexCopyPVR(&vin[0], &vout[0]);
                _glKosVertexCopyPVR(&vin[1], &vout[1]);
                _glKosVertexCopyPVR(&vin[2], &vout[2]);

                switch(clip) {
                    case FIRST_TWO_OUT:
                        _glKosVertexClipZNear3(&vout[0], &vout[2], &W[0], &W[2]);
                        _glKosVertexClipZNear3(&vout[1], &vout[2], &W[1], &W[2]);

                        break;

                    case FIRST_AND_LAST_OUT:
                        _glKosVertexClipZNear3(&vout[0], &vout[1], &W[0], &W[1]);
                        _glKosVertexClipZNear3(&vout[2], &vout[1], &W[2], &W[1]);

                        break;

                    case LAST_TWO_OUT:
                        _glKosVertexClipZNear3(&vout[1], &vout[0], &W[1], &W[0]);
                        _glKosVertexClipZNear3(&vout[2], &vout[0], &W[2], &W[0]);

                        break;
                }

                _glKosVertexPerspectiveDivide(&vout[0], W[0]);
                _glKosVertexPerspectiveDivide(&vout[1], W[1]);
                _glKosVertexPerspectiveDivide(&vout[2], W[2]);

                vout[0].flags = vout[1].flags = PVR_CMD_VERTEX;
                vout[2].flags = PVR_CMD_VERTEX_EOL;

                vout += 3;
                verts_out += 3;
                break;

            case 2:/* 2 Vertices of Triangle are inside of clip plane = output 2 Triangles */
                switch(clip) {
                    case FIRST:
                        _glKosVertexCopyPVR(&vin[0], &vout[0]);
                        _glKosVertexCopyPVR(&vin[1], &vout[1]);
                        _glKosVertexCopyPVR(&vin[0], &vout[2]);
                        _glKosVertexCopyPVR(&vin[2], &vout[3]);
                        W[3] = W[2];
                        W[2] = W[0];

                        break;

                    case SECOND:
                        _glKosVertexCopyPVR(&vin[1], &vout[0]);
                        _glKosVertexCopyPVR(&vin[2], &vout[1]);
                        _glKosVertexCopyPVR(&vin[1], &vout[2]);
                        _glKosVertexCopyPVR(&vin[0], &vout[3]);
                        W[3] = W[0];
                        W[0] = W[1];
                        W[1] = W[2];
                        W[2] = W[0];

                        break;

                    case THIRD:
                        _glKosVertexCopyPVR(&vin[2], &vout[0]);
                        _glKosVertexCopyPVR(&vin[0], &vout[1]);
                        _glKosVertexCopyPVR(&vin[2], &vout[2]);
                        _glKosVertexCopyPVR(&vin[1], &vout[3]);
                        W[3] = W[1];
                        W[1] = W[0];
                        W[0] = W[2];

                        break;
                }

                _glKosVertexClipZNear3(&vout[0], &vout[1], &W[0], &W[1]);
                _glKosVertexClipZNear3(&vout[2], &vout[3], &W[2], &W[3]);

                _glKosVertexPerspectiveDivide(&vout[0], W[0]);
                _glKosVertexPerspectiveDivide(&vout[1], W[1]);
                _glKosVertexPerspectiveDivide(&vout[2], W[2]);
                _glKosVertexPerspectiveDivide(&vout[3], W[3]);

                vout[0].flags = vout[1].flags = vout[2].flags = PVR_CMD_VERTEX;
                vout[3].flags = PVR_CMD_VERTEX_EOL;

                vout += 4;
                verts_out += 4;
                break;
        }

        ++vin;
    }

    return verts_out;
}

GLubyte _glKosClipTriTransformed(pvr_vertex_t *vin, float *w, pvr_vertex_t *vout) {
    unsigned short int clip = 0; /* Clip Code for current Triangle */
    unsigned char verts_in = 0; /* # of Vertices inside clip plane for current Triangle */
    float W[4] = { w[0], w[1], w[2] }; /* W Component for Perspective Divide */

    (vin[0].z >= CLIP_NEARZ) ? clip |= FIRST  : ++verts_in;
    (vin[1].z >= CLIP_NEARZ) ? clip |= SECOND : ++verts_in;
    (vin[2].z >= CLIP_NEARZ) ? clip |= THIRD  : ++verts_in;

    switch(verts_in) { /* Start by examining # of vertices inside clip plane */
        case 0: /* All Vertices of Triangle are Outside of clip plne */
            return 0;

        case 3: /* All Vertices of Triangle are inside of clip plne */
            _glKosVertexCopyPVR(&vin[0], &vout[0]);
            _glKosVertexCopyPVR(&vin[1], &vout[1]);
            _glKosVertexCopyPVR(&vin[2], &vout[2]);

            _glKosVertexPerspectiveDivide(&vout[0], W[0]);
            _glKosVertexPerspectiveDivide(&vout[1], W[1]);
            _glKosVertexPerspectiveDivide(&vout[2], W[2]);

            vout[0].flags = vout[1].flags = PVR_CMD_VERTEX;
            vout[2].flags = PVR_CMD_VERTEX_EOL;

            return 3;

        case 1:/* 1 Vertex of Triangle is inside of clip plane = output 1 Triangle */
            _glKosVertexCopyPVR(&vin[0], &vout[0]);
            _glKosVertexCopyPVR(&vin[1], &vout[1]);
            _glKosVertexCopyPVR(&vin[2], &vout[2]);

            switch(clip) {
                case FIRST_TWO_OUT:
                    _glKosVertexClipZNear3(&vout[0], &vout[2], &W[0], &W[2]);
                    _glKosVertexClipZNear3(&vout[1], &vout[2], &W[1], &W[2]);

                    break;

                case FIRST_AND_LAST_OUT:
                    _glKosVertexClipZNear3(&vout[0], &vout[1], &W[0], &W[1]);
                    _glKosVertexClipZNear3(&vout[2], &vout[1], &W[2], &W[1]);

                    break;

                case LAST_TWO_OUT:
                    _glKosVertexClipZNear3(&vout[1], &vout[0], &W[1], &W[0]);
                    _glKosVertexClipZNear3(&vout[2], &vout[0], &W[2], &W[0]);

                    break;
            }

            _glKosVertexPerspectiveDivide(&vout[0], W[0]);
            _glKosVertexPerspectiveDivide(&vout[1], W[1]);
            _glKosVertexPerspectiveDivide(&vout[2], W[2]);

            vout[0].flags = vout[1].flags = PVR_CMD_VERTEX;
            vout[2].flags = PVR_CMD_VERTEX_EOL;

            return 3;

        case 2:/* 2 Vertices of Triangle are inside of clip plane = output 2 Triangles */
            switch(clip) {
                case FIRST:
                    _glKosVertexCopyPVR(&vin[0], &vout[0]);
                    _glKosVertexCopyPVR(&vin[1], &vout[1]);
                    _glKosVertexCopyPVR(&vin[0], &vout[2]);
                    _glKosVertexCopyPVR(&vin[2], &vout[3]);
                    W[3] = W[2];
                    W[2] = W[0];

                    break;

                case SECOND:
                    _glKosVertexCopyPVR(&vin[1], &vout[0]);
                    _glKosVertexCopyPVR(&vin[2], &vout[1]);
                    _glKosVertexCopyPVR(&vin[1], &vout[2]);
                    _glKosVertexCopyPVR(&vin[0], &vout[3]);
                    W[3] = W[0];
                    W[0] = W[1];
                    W[1] = W[2];
                    W[2] = W[0];

                    break;

                case THIRD:
                    _glKosVertexCopyPVR(&vin[2], &vout[0]);
                    _glKosVertexCopyPVR(&vin[0], &vout[1]);
                    _glKosVertexCopyPVR(&vin[2], &vout[2]);
                    _glKosVertexCopyPVR(&vin[1], &vout[3]);
                    W[3] = W[1];
                    W[1] = W[0];
                    W[0] = W[2];

                    break;
            }

            _glKosVertexClipZNear3(&vout[0], &vout[1], &W[0], &W[1]);
            _glKosVertexClipZNear3(&vout[2], &vout[3], &W[2], &W[3]);

            _glKosVertexPerspectiveDivide(&vout[0], W[0]);
            _glKosVertexPerspectiveDivide(&vout[1], W[1]);
            _glKosVertexPerspectiveDivide(&vout[2], W[2]);
            _glKosVertexPerspectiveDivide(&vout[3], W[3]);

            vout[0].flags = vout[1].flags = vout[2].flags = PVR_CMD_VERTEX;
            vout[3].flags = PVR_CMD_VERTEX_EOL;

            return 4;
    }

    return 0;
}

unsigned int _glKosClipQuadsTransformed(pvr_vertex_t *vin, float *w, pvr_vertex_t *vout, unsigned int vertices) {
    unsigned int i, v = 0;
    pvr_vertex_t qv[3];
    float W[3];

    for(i = 0; i < vertices; i += 4) { /* Iterate all Quads, Rearranging into Triangle Strips */
        _glKosVertexCopyPVR(&vin[i + 0], &qv[0]);
        _glKosVertexCopyPVR(&vin[i + 2], &qv[1]);
        _glKosVertexCopyPVR(&vin[i + 3], &qv[2]);
        W[0] = w[0];
        W[1] = w[2];
        W[2] = w[3];

        v += _glKosClipTriTransformed(&vin[i], &w[i], &vout[v]);
        v += _glKosClipTriTransformed(&qv[i], &W[i], &vout[v]);
    }

    return v;
}
