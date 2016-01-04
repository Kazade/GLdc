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


static inline void _glKosVertexClipZNear2(pvr_vertex_t *v1, pvr_vertex_t *v2,
        GLfloat *w1, GLfloat *w2,
        glTexCoord *uva, glTexCoord *uvb) {
    GLfloat MAG = ((CLIP_NEARZ - v1->z) / (v2->z - v1->z));

    colorui *c1 = (colorui *)&v1->argb; /* GLubyte Color Component Pointer */
    colorui *c2 = (colorui *)&v2->argb;

    v1->x += (v2->x - v1->x) * MAG; /* Clip Vertex X, Y, Z Components */
    v1->y += (v2->y - v1->y) * MAG;
    v1->z += (v2->z - v1->z) * MAG;
    v1->u += (v2->u - v1->u) * MAG; /* Clip Vertex Texture Coordinates */
    v1->v += (v2->v - v1->v) * MAG;
    c1->a += (c2->a - c1->a) * MAG; /* Clip Vertex Color per ARGB component */
    c1->r += (c2->r - c1->r) * MAG;
    c1->g += (c2->g - c1->g) * MAG;
    c1->b += (c2->b - c1->b) * MAG;

    *w1 += (*w2 - *w1) * MAG;       /* Clip Vertex W Component */

    uva->u += (uvb->u - uva->u) * MAG; /* Clip Vertex Multi-Texture Coordinates */
    uva->v += (uvb->v - uva->v) * MAG;
}

static inline void _glKosVertexClipZNear3(pvr_vertex_t *v1, pvr_vertex_t *v2, float *w1, float *w2) {
    GLfloat MAG = ((CLIP_NEARZ - v1->z) / (v2->z - v1->z));

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

static inline void _glKosVertexPerspectiveDivide(pvr_vertex_t *dst, GLfloat w) {
    dst->z = 1.0f / w;
    dst->x *= dst->z;
    dst->y *= dst->z;
}

static inline GLubyte _glKosClipTriTransformed(pvr_vertex_t *src, GLfloat *w, pvr_vertex_t *dst) {
    GLushort clip = 0; /* Clip Code for current Triangle */
    GLubyte verts_in = 0; /* # of Vertices inside clip plane for current Triangle */
    GLfloat W[4] = { w[0], w[1], w[2] }; /* W Component for Perspective Divide */

    (src[0].z >= CLIP_NEARZ) ? clip |= FIRST  : ++verts_in;
    (src[1].z >= CLIP_NEARZ) ? clip |= SECOND : ++verts_in;
    (src[2].z >= CLIP_NEARZ) ? clip |= THIRD  : ++verts_in;

    switch(verts_in) { /* Start by examining # of vertices inside clip plane */
        case 0: /* All Vertices of Triangle are Outside of clip plne */
            return 0;

        case 3: /* All Vertices of Triangle are inside of clip plne */
            _glKosVertexCopyPVR(&src[0], &dst[0]);
            _glKosVertexCopyPVR(&src[1], &dst[1]);
            _glKosVertexCopyPVR(&src[2], &dst[2]);

            _glKosVertexPerspectiveDivide(&dst[0], W[0]);
            _glKosVertexPerspectiveDivide(&dst[1], W[1]);
            _glKosVertexPerspectiveDivide(&dst[2], W[2]);

            dst[0].flags = dst[1].flags = PVR_CMD_VERTEX;
            dst[2].flags = PVR_CMD_VERTEX_EOL;

            return 3;

        case 1:/* 1 Vertex of Triangle is inside of clip plane = output 1 Triangle */
            _glKosVertexCopyPVR(&src[0], &dst[0]);
            _glKosVertexCopyPVR(&src[1], &dst[1]);
            _glKosVertexCopyPVR(&src[2], &dst[2]);

            switch(clip) {
                case FIRST_TWO_OUT:
                    _glKosVertexClipZNear3(&dst[0], &dst[2], &W[0], &W[2]);
                    _glKosVertexClipZNear3(&dst[1], &dst[2], &W[1], &W[2]);

                    break;

                case FIRST_AND_LAST_OUT:
                    _glKosVertexClipZNear3(&dst[0], &dst[1], &W[0], &W[1]);
                    _glKosVertexClipZNear3(&dst[2], &dst[1], &W[2], &W[1]);

                    break;

                case LAST_TWO_OUT:
                    _glKosVertexClipZNear3(&dst[1], &dst[0], &W[1], &W[0]);
                    _glKosVertexClipZNear3(&dst[2], &dst[0], &W[2], &W[0]);

                    break;
            }

            _glKosVertexPerspectiveDivide(&dst[0], W[0]);
            _glKosVertexPerspectiveDivide(&dst[1], W[1]);
            _glKosVertexPerspectiveDivide(&dst[2], W[2]);

            dst[0].flags = dst[1].flags = PVR_CMD_VERTEX;
            dst[2].flags = PVR_CMD_VERTEX_EOL;

            return 3;

        case 2:/* 2 Vertices of Triangle are inside of clip plane = output 2 Triangles */
            switch(clip) {
                case FIRST:
                    _glKosVertexCopyPVR(&src[0], &dst[0]);
                    _glKosVertexCopyPVR(&src[1], &dst[1]);
                    _glKosVertexCopyPVR(&src[0], &dst[2]);
                    _glKosVertexCopyPVR(&src[2], &dst[3]);
                    W[3] = W[2];
                    W[2] = W[0];

                    break;

                case SECOND:
                    _glKosVertexCopyPVR(&src[1], &dst[0]);
                    _glKosVertexCopyPVR(&src[2], &dst[1]);
                    _glKosVertexCopyPVR(&src[1], &dst[2]);
                    _glKosVertexCopyPVR(&src[0], &dst[3]);
                    W[3] = W[0];
                    W[0] = W[1];
                    W[1] = W[2];
                    W[2] = W[0];

                    break;

                case THIRD:
                    _glKosVertexCopyPVR(&src[2], &dst[0]);
                    _glKosVertexCopyPVR(&src[0], &dst[1]);
                    _glKosVertexCopyPVR(&src[2], &dst[2]);
                    _glKosVertexCopyPVR(&src[1], &dst[3]);
                    W[3] = W[1];
                    W[1] = W[0];
                    W[0] = W[2];

                    break;
            }

            _glKosVertexClipZNear3(&dst[0], &dst[1], &W[0], &W[1]);
            _glKosVertexClipZNear3(&dst[2], &dst[3], &W[2], &W[3]);

            _glKosVertexPerspectiveDivide(&dst[0], W[0]);
            _glKosVertexPerspectiveDivide(&dst[1], W[1]);
            _glKosVertexPerspectiveDivide(&dst[2], W[2]);
            _glKosVertexPerspectiveDivide(&dst[3], W[3]);

            dst[0].flags = dst[1].flags = dst[2].flags = PVR_CMD_VERTEX;
            dst[3].flags = PVR_CMD_VERTEX_EOL;

            return 4;
    }

    return 0;
}

GLuint _glKosClipTrianglesTransformed(pvr_vertex_t *src, GLfloat *w, pvr_vertex_t *dst, GLuint count) {
    GLuint verts_out = 0;
    GLuint i;

    for(i = 0; i < count; i += 3)
        verts_out += _glKosClipTriTransformed(&src[i], &w[i], &dst[verts_out]);

    return verts_out;
}

GLuint _glKosClipTriangleStripTransformed(pvr_vertex_t *src, GLfloat *w, pvr_vertex_t *dst, GLuint count) {
    GLuint verts_out = 0;
    GLuint i;

    for(i = 0; i < (count - 2); i ++)
        verts_out += _glKosClipTriTransformed(&src[i], &w[i], &dst[verts_out]);

    return verts_out;
}

unsigned int _glKosClipQuadsTransformed(pvr_vertex_t *src, GLfloat *w, pvr_vertex_t *dst, GLuint count) {
    GLuint i, verts_out = 0;
    pvr_vertex_t qv[3];
    GLfloat W[3];

    for(i = 0; i < count; i += 4) { /* Iterate all Quads, Rearranging into Triangle Strips */
        _glKosVertexCopyPVR(&src[i + 0], &qv[0]);
        _glKosVertexCopyPVR(&src[i + 2], &qv[1]);
        _glKosVertexCopyPVR(&src[i + 3], &qv[2]);
        W[0] = w[0];
        W[1] = w[2];
        W[2] = w[3];

        verts_out += _glKosClipTriTransformed(&src[i], &w[i], &dst[verts_out]);
        verts_out += _glKosClipTriTransformed(qv, W, &dst[verts_out]);
    }

    return verts_out;
}

static inline GLubyte _glKosClipTriTransformedMT(pvr_vertex_t *src, float *w, pvr_vertex_t *dst,
        GLfloat *uvsrc, glTexCoord *uvdst, GLuint uv_src_stride) {
    GLushort clip = 0; /* Clip Code for current Triangle */
    GLubyte verts_in = 0; /* # of Vertices inside clip plane for current Triangle */
    GLfloat W[4] = { w[0], w[1], w[2] }; /* W Component for Perspective Divide */

    (src[0].z >= CLIP_NEARZ) ? clip |= FIRST  : ++verts_in;
    (src[1].z >= CLIP_NEARZ) ? clip |= SECOND : ++verts_in;
    (src[2].z >= CLIP_NEARZ) ? clip |= THIRD  : ++verts_in;

    switch(verts_in) { /* Start by examining # of vertices inside clip plane */
        case 0: /* All Vertices of Triangle are Outside of clip plane */
            return 0;

        case 3: /* All Vertices of Triangle are Inside of clip plane */
            _glKosVertexCopyPVR(&src[0], &dst[0]);
            _glKosVertexCopyPVR(&src[1], &dst[1]);
            _glKosVertexCopyPVR(&src[2], &dst[2]);

            _glKosTexCoordCopy((glTexCoord *)&uvsrc[0], &uvdst[0]);
            _glKosTexCoordCopy((glTexCoord *)&uvsrc[uv_src_stride], &uvdst[1]);
            _glKosTexCoordCopy((glTexCoord *)&uvsrc[uv_src_stride + uv_src_stride], &uvdst[2]);

            _glKosVertexPerspectiveDivide(&dst[0], W[0]);
            _glKosVertexPerspectiveDivide(&dst[1], W[1]);
            _glKosVertexPerspectiveDivide(&dst[2], W[2]);

            dst[0].flags = dst[1].flags = PVR_CMD_VERTEX;
            dst[2].flags = PVR_CMD_VERTEX_EOL;

            return 3;

        case 1:/* 1 Vertex of Triangle is Inside of clip plane = output 1 Triangle */
            _glKosVertexCopyPVR(&src[0], &dst[0]);
            _glKosVertexCopyPVR(&src[1], &dst[1]);
            _glKosVertexCopyPVR(&src[2], &dst[2]);

            _glKosTexCoordCopy((glTexCoord *)&uvsrc[0], &uvdst[0]);
            _glKosTexCoordCopy((glTexCoord *)&uvsrc[uv_src_stride], &uvdst[1]);
            _glKosTexCoordCopy((glTexCoord *)&uvsrc[uv_src_stride + uv_src_stride], &uvdst[2]);

            switch(clip) {
                case FIRST_TWO_OUT:
                    _glKosVertexClipZNear2(&dst[0], &dst[2], &W[0], &W[2], &uvdst[0], &uvdst[2]);
                    _glKosVertexClipZNear2(&dst[1], &dst[2], &W[1], &W[2], &uvdst[1], &uvdst[2]);

                    break;

                case FIRST_AND_LAST_OUT:
                    _glKosVertexClipZNear2(&dst[0], &dst[1], &W[0], &W[1], &uvdst[0], &uvdst[1]);
                    _glKosVertexClipZNear2(&dst[2], &dst[1], &W[2], &W[1], &uvdst[2], &uvdst[1]);

                    break;

                case LAST_TWO_OUT:
                    _glKosVertexClipZNear2(&dst[1], &dst[0], &W[1], &W[0], &uvdst[1], &uvdst[0]);
                    _glKosVertexClipZNear2(&dst[2], &dst[0], &W[2], &W[0], &uvdst[2], &uvdst[0]);

                    break;
            }

            _glKosVertexPerspectiveDivide(&dst[0], W[0]);
            _glKosVertexPerspectiveDivide(&dst[1], W[1]);
            _glKosVertexPerspectiveDivide(&dst[2], W[2]);

            dst[0].flags = dst[1].flags = PVR_CMD_VERTEX;
            dst[2].flags = PVR_CMD_VERTEX_EOL;

            return 3;

        case 2:/* 2 Vertices of Triangle are inside of clip plane = output 2 Triangles */
            switch(clip) {
                case FIRST:
                    _glKosVertexCopyPVR(&src[0], &dst[0]);
                    _glKosVertexCopyPVR(&src[1], &dst[1]);
                    _glKosVertexCopyPVR(&src[0], &dst[2]);
                    _glKosVertexCopyPVR(&src[2], &dst[3]);

                    _glKosTexCoordCopy((glTexCoord *)&uvsrc[0], &uvdst[0]);
                    _glKosTexCoordCopy((glTexCoord *)&uvsrc[uv_src_stride], &uvdst[1]);
                    _glKosTexCoordCopy((glTexCoord *)&uvsrc[0], &uvdst[2]);
                    _glKosTexCoordCopy((glTexCoord *)&uvsrc[uv_src_stride + uv_src_stride], &uvdst[3]);

                    W[3] = W[2];
                    W[2] = W[0];

                    break;

                case SECOND:
                    _glKosVertexCopyPVR(&src[1], &dst[0]);
                    _glKosVertexCopyPVR(&src[2], &dst[1]);
                    _glKosVertexCopyPVR(&src[1], &dst[2]);
                    _glKosVertexCopyPVR(&src[0], &dst[3]);

                    _glKosTexCoordCopy((glTexCoord *)&uvsrc[uv_src_stride], &uvdst[0]);
                    _glKosTexCoordCopy((glTexCoord *)&uvsrc[uv_src_stride + uv_src_stride], &uvdst[1]);
                    _glKosTexCoordCopy((glTexCoord *)&uvsrc[uv_src_stride], &uvdst[2]);
                    _glKosTexCoordCopy((glTexCoord *)&uvsrc[0], &uvdst[3]);

                    W[3] = W[0];
                    W[0] = W[1];
                    W[1] = W[2];
                    W[2] = W[0];

                    break;

                case THIRD:
                    _glKosVertexCopyPVR(&src[2], &dst[0]);
                    _glKosVertexCopyPVR(&src[0], &dst[1]);
                    _glKosVertexCopyPVR(&src[2], &dst[2]);
                    _glKosVertexCopyPVR(&src[1], &dst[3]);

                    _glKosTexCoordCopy((glTexCoord *)&uvsrc[uv_src_stride + uv_src_stride], &uvdst[0]);
                    _glKosTexCoordCopy((glTexCoord *)&uvsrc[0], &uvdst[1]);
                    _glKosTexCoordCopy((glTexCoord *)&uvsrc[uv_src_stride + uv_src_stride], &uvdst[2]);
                    _glKosTexCoordCopy((glTexCoord *)&uvsrc[uv_src_stride], &uvdst[3]);

                    W[3] = W[1];
                    W[1] = W[0];
                    W[0] = W[2];

                    break;
            }

            _glKosVertexClipZNear2(&dst[0], &dst[1], &W[0], &W[1], &uvdst[0], &uvdst[1]);
            _glKosVertexClipZNear2(&dst[2], &dst[3], &W[2], &W[3], &uvdst[2], &uvdst[3]);

            _glKosVertexPerspectiveDivide(&dst[0], W[0]);
            _glKosVertexPerspectiveDivide(&dst[1], W[1]);
            _glKosVertexPerspectiveDivide(&dst[2], W[2]);
            _glKosVertexPerspectiveDivide(&dst[3], W[3]);

            dst[0].flags = dst[1].flags = dst[2].flags = PVR_CMD_VERTEX;
            dst[3].flags = PVR_CMD_VERTEX_EOL;

            return 4;
    }

    return 0;
}

GLuint _glKosClipTrianglesTransformedMT(pvr_vertex_t *src, float *w, pvr_vertex_t *dst,
                                        GLfloat *uvsrc, glTexCoord *uvdst, GLuint uv_src_stride,
                                        GLuint count) {
    GLuint verts_out = 0;
    GLuint i;

    for(i = 0; i < count; i += 3)
        verts_out += _glKosClipTriTransformedMT(&src[i], &w[i], &dst[verts_out],
                                                &uvsrc[i * uv_src_stride], &uvdst[verts_out], uv_src_stride);

    return verts_out;
}

GLuint _glKosClipTriangleStripTransformedMT(pvr_vertex_t *src, float *w, pvr_vertex_t *dst,
        GLfloat *uvsrc, glTexCoord *uvdst, GLuint uv_src_stride, GLuint count) {
    GLuint verts_out = 0;
    GLuint i;

    for(i = 0; i < (count - 2); i ++)
        verts_out += _glKosClipTriTransformedMT(&src[i], &w[i], &dst[verts_out],
                                                &uvsrc[i * uv_src_stride], &uvdst[verts_out], uv_src_stride);

    return verts_out;
}

GLuint _glKosClipQuadsTransformedMT(pvr_vertex_t *src, float *w, pvr_vertex_t *dst,
                                    GLfloat *uvsrc, glTexCoord *uvdst, GLuint uv_src_stride, GLuint count) {
    GLuint i, verts_out = 0;
    pvr_vertex_t qv[3];
    glTexCoord   uv[3];
    GLfloat W[3];

    for(i = 0; i < count; i += 4) {
        _glKosVertexCopyPVR(&src[i + 0], &qv[0]);
        _glKosVertexCopyPVR(&src[i + 2], &qv[1]);
        _glKosVertexCopyPVR(&src[i + 3], &qv[2]);

        _glKosTexCoordCopy((glTexCoord *)&uvsrc[i * uv_src_stride], &uv[0]);
        _glKosTexCoordCopy((glTexCoord *)&uvsrc[i * uv_src_stride + (uv_src_stride * 2)], &uv[1]);
        _glKosTexCoordCopy((glTexCoord *)&uvsrc[i * uv_src_stride + (uv_src_stride * 3)], &uv[2]);

        W[0] = w[0];
        W[1] = w[2];
        W[2] = w[3];

        verts_out += _glKosClipTriTransformedMT(&src[i], &w[i], &dst[verts_out],
                                                &uvsrc[i * uv_src_stride], &uvdst[verts_out], uv_src_stride);
        verts_out += _glKosClipTriTransformedMT(qv, W, &dst[verts_out],
                                                (GLfloat *)uv, &uvdst[verts_out], 2);
    }

    return verts_out;
}