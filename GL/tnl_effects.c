#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

#include "private.h"
#include "platform.h"

#define TNL_FX_LIGHTING 0x01
#define TNL_FX_TEXTURE  0x02
#define TNL_FX_COLOR    0x04
static int TNL_EFFECTS;

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

    if(TNL_EFFECTS & TNL_FX_LIGHTING) {
        _glMatrixLoadModelView();
    } else {
        _glMatrixLoadModelViewProjection();
    }
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


static void mat_transform_normal3(VertexExtra* extra, const uint32_t count) {
    ITERATE(count) {
        TransformNormalNoMod(extra->nxyz, extra->nxyz);
        extra++;
    }
}

static void lightingEffect(SubmissionTarget* target) {
    /* Perform lighting calculations and manipulate the colour */
    Vertex* vertex = _glSubmissionTargetStart(target);
    VertexExtra* extra = aligned_vector_at(target->extras, 0);

    _glMatrixLoadNormal();
    mat_transform_normal3(extra, target->count);

    _glPerformLighting(vertex, extra, target->count);
}

void _glTnlUpdateLighting(void) {
    if (_glIsLightingEnabled()) {
         TNL_EFFECTS |= TNL_FX_LIGHTING;
    } else {
         TNL_EFFECTS &= ~TNL_FX_LIGHTING;
    }
}

void _glTnlApplyEffects(SubmissionTarget* target) {
    if (!TNL_EFFECTS) return;

    if (TNL_EFFECTS & TNL_FX_LIGHTING)
        lightingEffect(target);

    if (TNL_EFFECTS & TNL_FX_LIGHTING) {
        /* OK eye-space work done, now move into clip space */
        _glMatrixLoadProjection();
        transformVertices(target);
    }
}
