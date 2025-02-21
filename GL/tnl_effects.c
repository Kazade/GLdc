#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

#include "private.h"
#include "platform.h"

#define MAX_TNL_EFFECTS 3

static struct tnl_effect {
    GLint flags;
    TnlEffect func;
} TNL_EFFECTS[MAX_TNL_EFFECTS];

static int TNL_COUNT;
static GLboolean TNL_VIEW;

#define ITERATE(count) \
    GLuint i = count; \
    while(i--)

static void updateEffectList(void) {
     TNL_VIEW = GL_FALSE;

     ITERATE(TNL_COUNT) {
          if (TNL_EFFECTS[i].flags == TNL_EFFECT_VIEW_SPACE) TNL_VIEW = true;
     }
}

void _glTnlAddEffect(GLint flags, TnlEffect func) {
     if (TNL_COUNT == MAX_TNL_EFFECTS) return;
     
     TNL_EFFECTS[TNL_COUNT].flags = flags;
     TNL_EFFECTS[TNL_COUNT].func  = func;

     TNL_COUNT++;
     updateEffectList();
}

void _glTnlRemoveEffect(TnlEffect func) {
     int i, j;

     for (i = TNL_COUNT - 1; i >= 0; i--) {
         if (TNL_EFFECTS[i].func != func) continue;

         for(j = i; j < TNL_COUNT - 1; j++) {
             TNL_EFFECTS[j] = TNL_EFFECTS[j + 1];
         }
         TNL_COUNT--;
     }
     updateEffectList();
}

void _glTnlLoadMatrix(void) {
    /* If we're lighting, then we need to do some work in
     * eye-space, so we only transform vertices by the modelview
     * matrix, and then later multiply by projection.
     *
     * If we're not doing lighting though we can optimise by taking
     * vertices straight to clip-space */

    if(TNL_VIEW) {
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

void _glTnlApplyEffects(SubmissionTarget* target) {
    if (!TNL_COUNT) return;

    struct tnl_effect* e = TNL_EFFECTS;
    ITERATE(TNL_COUNT) {
         e->func(target);
         e++;
    }

    if (!TNL_VIEW) return;
    /* OK eye-space work done, now move into clip space */
    _glMatrixLoadProjection();
    transformVertices(target);
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
         _glTnlAddEffect(TNL_EFFECT_VIEW_SPACE, lightingEffect);
    } else {
         _glTnlRemoveEffect(lightingEffect);
    }
}
