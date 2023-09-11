
#include "../containers/aligned_vector.h"
#include "private.h"

PolyList OP_LIST;
PolyList PT_LIST;
PolyList TR_LIST;

/**
 *  FAST_MODE will use invW for all Z coordinates sent to the
 *  GPU.
 *
 *  This will break orthographic mode so default is FALSE
 **/

#define FAST_MODE GL_FALSE

GLboolean AUTOSORT_ENABLED = GL_FALSE;

PolyList* _glOpaquePolyList() {
    return &OP_LIST;
}

PolyList* _glPunchThruPolyList() {
    return &PT_LIST;
}

PolyList *_glTransparentPolyList() {
    return &TR_LIST;
}

void APIENTRY glFlush() {

}

void APIENTRY glFinish() {

}


void APIENTRY glKosInitConfig(GLdcConfig* config) {
    config->autosort_enabled = GL_FALSE;
    config->fsaa_enabled = GL_FALSE;

    config->initial_op_capacity = 1024 * 3;
    config->initial_pt_capacity = 512 * 3;
    config->initial_tr_capacity = 1024 * 3;
    config->initial_immediate_capacity = 1024 * 3;

    // RGBA4444 is the fastest general format - 8888 will cause a perf issue
    config->internal_palette_format = GL_RGBA4;

    config->texture_twiddle = GL_TRUE;
}

static bool _initialized = false;

void APIENTRY glKosInitEx(GLdcConfig* config) {
    if(_initialized) {
        return;
    }

    _initialized = true;

    TRACE();

    printf("\nWelcome to GLdc! Git revision: %s\n\n", GLDC_VERSION);

    InitGPU(config->autosort_enabled, config->fsaa_enabled);

    AUTOSORT_ENABLED = config->autosort_enabled;

    _glInitSubmissionTarget();
    _glInitMatrices();
    _glInitAttributePointers();
    _glInitContext();
    _glInitLights();
    _glInitImmediateMode(config->initial_immediate_capacity);
    _glInitFramebuffers();

    _glSetInternalPaletteFormat(config->internal_palette_format);

    _glInitTextures();

    if(config->texture_twiddle) {
        glEnable(GL_TEXTURE_TWIDDLE_KOS);
    }

    OP_LIST.list_type = GPU_LIST_OP_POLY;
    PT_LIST.list_type = GPU_LIST_PT_POLY;
    TR_LIST.list_type = GPU_LIST_TR_POLY;

    aligned_vector_init(&OP_LIST.vector, sizeof(Vertex));
    aligned_vector_init(&PT_LIST.vector, sizeof(Vertex));
    aligned_vector_init(&TR_LIST.vector, sizeof(Vertex));

    aligned_vector_reserve(&OP_LIST.vector, config->initial_op_capacity);
    aligned_vector_reserve(&PT_LIST.vector, config->initial_pt_capacity);
    aligned_vector_reserve(&TR_LIST.vector, config->initial_tr_capacity);
}

void APIENTRY glKosShutdown() {
    aligned_vector_clear(&OP_LIST.vector);
    aligned_vector_clear(&PT_LIST.vector);
    aligned_vector_clear(&TR_LIST.vector);
}

void APIENTRY glKosInit() {
    GLdcConfig config;
    glKosInitConfig(&config);
    glKosInitEx(&config);
}

void APIENTRY glKosSwapBuffers() {
    TRACE();

    SceneBegin();
        if(aligned_vector_header(&OP_LIST.vector)->size > 2) {
            SceneListBegin(GPU_LIST_OP_POLY);
            SceneListSubmit((Vertex*) aligned_vector_front(&OP_LIST.vector), aligned_vector_size(&OP_LIST.vector));
            SceneListFinish();
        }

        if(aligned_vector_header(&PT_LIST.vector)->size > 2) {
            SceneListBegin(GPU_LIST_PT_POLY);
            SceneListSubmit((Vertex*) aligned_vector_front(&PT_LIST.vector), aligned_vector_size(&PT_LIST.vector));
            SceneListFinish();
        }

        if(aligned_vector_header(&TR_LIST.vector)->size > 2) {
            SceneListBegin(GPU_LIST_TR_POLY);
            SceneListSubmit((Vertex*) aligned_vector_front(&TR_LIST.vector), aligned_vector_size(&TR_LIST.vector));
            SceneListFinish();
        }
    SceneFinish();

    aligned_vector_clear(&OP_LIST.vector);
    aligned_vector_clear(&PT_LIST.vector);
    aligned_vector_clear(&TR_LIST.vector);

    _glApplyScissor(true);
}
