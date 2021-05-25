
#include "../containers/aligned_vector.h"
#include "private.h"

PolyList OP_LIST;
PolyList PT_LIST;
PolyList TR_LIST;

/** Don't fully comply to the GL standard to make some performance
 *  gains. Specifically glDepthRange will be ignored, and the final
 *  Z coordinate will be invW and not between 0 and 1.
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
    config->internal_palette_format = GL_RGBA4;
}

void APIENTRY glKosInitEx(GLdcConfig* config) {
    TRACE();

    printf("\nWelcome to GLdc! Git revision: %s\n\n", GLDC_VERSION);

    InitGPU(config->autosort_enabled, config->fsaa_enabled);

    AUTOSORT_ENABLED = config->autosort_enabled;

    _glInitMatrices();
    _glInitAttributePointers();
    _glInitContext();
    _glInitLights();
    _glInitImmediateMode(config->initial_immediate_capacity);
    _glInitFramebuffers();

    _glSetInternalPaletteFormat(config->internal_palette_format);

    _glInitTextures();

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

void APIENTRY glKosInit() {
    GLdcConfig config;
    glKosInitConfig(&config);
    glKosInitEx(&config);
}

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

GL_FORCE_INLINE bool glIsVertex(const float flags) {
    return flags == GPU_CMD_VERTEX_EOL || flags == GPU_CMD_VERTEX;
}


GL_FORCE_INLINE void glPerspectiveDivideStandard(void* src, uint32_t n) {
    TRACE();

    /* Perform perspective divide on each vertex */
    Vertex* vertex = (Vertex*) src;
    PREFETCH(vertex + 1);

    const float h = GetVideoMode()->height;

    while(n--) {
        PREFETCH(vertex + 2);

        if(likely(glIsVertex(vertex->flags))) {
            const float f = MATH_Fast_Invert(vertex->w);

            /* Convert to NDC and apply viewport */
            vertex->xyz[0] = __builtin_fmaf(
                VIEWPORT.hwidth, vertex->xyz[0] * f, VIEWPORT.x_plus_hwidth
            );

            vertex->xyz[1] = h - __builtin_fmaf(
                VIEWPORT.hheight, vertex->xyz[1] * f, VIEWPORT.y_plus_hheight
            );

            /* FIXME: Apply depth range */

            /* After multiplying by 'f', the Z coordinate is between
            * -1 and 1. We then need to shift it into a value > 0.00001f
            * where the larger value becomes smaller and vice-versa (because
            * the PVR works backwards).
            *
            * If we multipled the lowest value (-1) by -1 it becomes 1, if
            * we multiply the lowest value (1) by -1 it becomes, then we need
            * to add 1 to get it in the range 0 - 2. Then we add a little offset
            * and this approach means we can just use FMAC.
            * */
            vertex->xyz[2] = __builtin_fmaf((vertex->xyz[2] * f), -1.0f, 1.00001f);
        }

        ++vertex;
    }
}

GL_FORCE_INLINE void glPerspectiveDivideFastMode(void* src, uint32_t n) {
    TRACE();

    /* Perform perspective divide on each vertex */
    Vertex* vertex = (Vertex*) src;

    const float h = GetVideoMode()->height;

    while(n--) {
        PREFETCH(vertex + 1);

        if(likely(glIsVertex(vertex->flags))) {
            const float f = MATH_Fast_Invert(vertex->w);

            /* Convert to NDC and apply viewport */
            vertex->xyz[0] = MATH_fmac(
                VIEWPORT.hwidth, vertex->xyz[0] * f, VIEWPORT.x_plus_hwidth
            );

            vertex->xyz[1] = h - MATH_fmac(
                VIEWPORT.hheight, vertex->xyz[1] * f, VIEWPORT.y_plus_hheight
            );

            vertex->xyz[2] = f;
        }

        ++vertex;
    }
}

GL_FORCE_INLINE void glPerspectiveDivide(void* src, uint32_t n) {
#if FAST_MODE
        glPerspectiveDivideFastMode(src, n);
#else
        glPerspectiveDivideStandard(src, n);
#endif
}

void APIENTRY glKosSwapBuffers() {
    TRACE();

    SceneBegin();
        SceneListBegin(GPU_LIST_OP_POLY);
        glPerspectiveDivide(OP_LIST.vector.data, OP_LIST.vector.size);
        SceneListSubmit(OP_LIST.vector.data, OP_LIST.vector.size);
        SceneListFinish();

        SceneListBegin(GPU_LIST_PT_POLY);
        glPerspectiveDivide(PT_LIST.vector.data, PT_LIST.vector.size);
        SceneListSubmit(PT_LIST.vector.data, PT_LIST.vector.size);
        SceneListFinish();

        SceneListBegin(GPU_LIST_TR_POLY);
        glPerspectiveDivide(TR_LIST.vector.data, TR_LIST.vector.size);
        SceneListSubmit(TR_LIST.vector.data, TR_LIST.vector.size);
        SceneListFinish();
    SceneFinish();

    aligned_vector_clear(&OP_LIST.vector);
    aligned_vector_clear(&PT_LIST.vector);
    aligned_vector_clear(&TR_LIST.vector);

    _glApplyScissor(true);
}
