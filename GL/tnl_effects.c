#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

#include "types.h"
#include "private.h"
#include "platform.h"

static int TNL_EFFECTS, TNL_LIGHTING, TNL_TEXTURE, TNL_COLOR;

#define ITERATE(count) \
    GLuint i = count; \
    while(i--)

void _glTnlLoadMatrix(void) {
    /* If we're lighting, then we need to do some work in
     * eye-space, so we only transform vertices by the modelview
     * matrix, and then later multiply by projection.
     *
     * If we're not doing lighting though we can optimise by taking
     * vertices straight to clip-space */

    if(TNL_LIGHTING) {
        _glMatrixLoadModelView();
    } else {
        _glMatrixLoadModelViewProjection();
    }
}

static void updateEffects(void) {
    TNL_EFFECTS = TNL_LIGHTING | TNL_TEXTURE | TNL_COLOR;
}

static void transformVertices(SubmissionTarget* target) {
    TRACE();

    /* Perform modelview transform, storing W */
    Vertex* it     = _glSubmissionTargetStart(target);
    uint32_t count = target->count;

    ITERATE(count) {
        TransformVertex(it->xyz[0], it->xyz[1], it->xyz[2], it->w,
                        it->xyz, &it->w);
        it++;
    }
}


static void mat_transform_normal3(Vertex* v, const uint32_t count) {
    ITERATE(count) {
        float nxyz[3];
        _glUnpackNormal(v->nxyz, nxyz);
        TransformNormalNoMod(nxyz, nxyz);
        v->nxyz = _glPackNormal(nxyz);
        v++;
    }
}

static void lightingEffect(SubmissionTarget* target) {
    /* Perform lighting calculations and manipulate the colour */
    Vertex* vertex = _glSubmissionTargetStart(target);

    _glMatrixLoadNormal();
    mat_transform_normal3(vertex, target->count);

    _glPerformLighting(vertex, target->count);
}

void _glTnlUpdateLighting(void) {
    TNL_LIGHTING = _glIsLightingEnabled();
    updateEffects();
}


static void textureEffect(SubmissionTarget* target) {
    Matrix4x4* m = _glGetTextureMatrix();
    UploadMatrix4x4(m);
    float coords[4];

    Vertex* it     = _glSubmissionTargetStart(target);
    uint32_t count = target->count;

    ITERATE(count) {
        TransformVertex(it->uv[0], it->uv[1], 0.0f, 1.0f, coords, &coords[3]);
        it->uv[0] = coords[0];
        it->uv[1] = coords[1];
        it++;
    }
}

void _glTnlUpdateTextureMatrix(void) {
    Matrix4x4* m = _glGetTextureMatrix();
    TNL_TEXTURE  = !_glIsIdentity(m);
    updateEffects();
}


static void colorEffect(SubmissionTarget* target) {
    Matrix4x4* m = _glGetColorMatrix();
    UploadMatrix4x4(m);
    float coords[4];

    Vertex* it     = _glSubmissionTargetStart(target);
    uint32_t count = target->count;

    ITERATE(count) {
        TransformVertex(it->argb[2], it->argb[1], it->argb[0], it->argb[3], coords, &coords[3]);
        it->argb[R8IDX] = coords[0];
        it->argb[G8IDX] = coords[1];
        it->argb[B8IDX] = coords[2];
        it->argb[A8IDX] = coords[3];
        it++;
    }
}

void _glTnlUpdateColorMatrix(void) {
    Matrix4x4* m = _glGetColorMatrix();
    TNL_COLOR    = !_glIsIdentity(m);
    updateEffects();
}


void _glTnlApplyEffects(SubmissionTarget* target) {
    if (!TNL_EFFECTS) return;

    if (TNL_LIGHTING)
        lightingEffect(target);
    if (TNL_TEXTURE)
        textureEffect(target);
    if (TNL_COLOR)
        colorEffect(target);

    if (TNL_LIGHTING) {
        /* OK eye-space work done, now move into clip space */
        _glMatrixLoadProjection();
        transformVertices(target);
    }
}
