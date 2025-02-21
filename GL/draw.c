#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

#include "private.h"
#include "platform.h"

GLubyte ACTIVE_CLIENT_TEXTURE;

extern GLboolean AUTOSORT_ENABLED;

#define ITERATE(count) \
    GLuint i = count; \
    while(i--)


typedef GLuint (*IndexParseFunc)(const GLubyte* in);

static inline GLuint _parseUByteIndex(const GLubyte* in) {
    return (GLuint) *in;
}

static inline GLuint _parseUIntIndex(const GLubyte* in) {
    return *((GLuint*) in);
}

static inline GLuint _parseUShortIndex(const GLubyte* in) {
    return *((GLshort*) in);
}

GL_FORCE_INLINE GLsizei index_size(GLenum type) {
    switch(type) {
    case GL_UNSIGNED_BYTE: return sizeof(GLubyte);
    case GL_UNSIGNED_SHORT: return sizeof(GLushort);
    case GL_UNSIGNED_INT: return sizeof(GLuint);
    default: return sizeof(GLushort);
    }
}

GL_FORCE_INLINE IndexParseFunc _calcParseIndexFunc(GLenum type) {
    switch(type) {
    case GL_UNSIGNED_BYTE:
        return &_parseUByteIndex;
    break;
    case GL_UNSIGNED_INT:
        return &_parseUIntIndex;
    break;
    case GL_UNSIGNED_SHORT:
    default:
        break;
    }

    return &_parseUShortIndex;
}


/* There was a bug in this macro that shipped with Kos
 * which has now been fixed. But just in case...
 */
#undef mat_trans_single3_nodiv
#define mat_trans_single3_nodiv(x, y, z) { \
    register float __x __asm__("fr12") = (x); \
    register float __y __asm__("fr13") = (y); \
    register float __z __asm__("fr14") = (z); \
    __asm__ __volatile__( \
                          "fldi1 fr15\n" \
                          "ftrv  xmtrx, fv12\n" \
                          : "=f" (__x), "=f" (__y), "=f" (__z) \
                          : "0" (__x), "1" (__y), "2" (__z) \
                          : "fr15"); \
    x = __x; y = __y; z = __z; \
}


/* FIXME: Is this right? Shouldn't it be fr12->15? */
#undef mat_trans_normal3
#define mat_trans_normal3(x, y, z) { \
    register float __x __asm__("fr8") = (x); \
    register float __y __asm__("fr9") = (y); \
    register float __z __asm__("fr10") = (z); \
    __asm__ __volatile__( \
                          "fldi0 fr11\n" \
                          "ftrv  xmtrx, fv8\n" \
                          : "=f" (__x), "=f" (__y), "=f" (__z) \
                          : "0" (__x), "1" (__y), "2" (__z) \
                          : "fr11"); \
    x = __x; y = __y; z = __z; \
}

GL_FORCE_INLINE PolyHeader *_glSubmissionTargetHeader(SubmissionTarget* target) {
    gl_assert(target->header_offset < aligned_vector_size(&target->output->vector));
    return aligned_vector_at(&target->output->vector, target->header_offset);
}

GL_INLINE_DEBUG Vertex* _glSubmissionTargetStart(SubmissionTarget* target) {
    gl_assert(target->start_offset < aligned_vector_size(&target->output->vector));
    return aligned_vector_at(&target->output->vector, target->start_offset);
}

Vertex* _glSubmissionTargetEnd(SubmissionTarget* target) {
    return _glSubmissionTargetStart(target) + target->count;
}

GL_FORCE_INLINE void genTriangles(Vertex* output, GLuint count) {
    Vertex* it = output + 2;

    GLuint i;
    for(i = 0; i < count; i += 3) {
        it->flags = GPU_CMD_VERTEX_EOL;
        it += 3;
    }
}

GL_FORCE_INLINE void genQuads(Vertex* output, GLuint count) {
    Vertex* pen = output + 2;
    Vertex* final = output + 3;
    GLuint i = count >> 2;
    while(i--) {
        PREFETCH(pen + 4);
        PREFETCH(final + 4);

        swapVertex(pen, final);
        final->flags = GPU_CMD_VERTEX_EOL;

        pen += 4;
        final += 4;
    }
}

GL_FORCE_INLINE void genTriangleStrip(Vertex* output, GLuint count) {
    output[count - 1].flags = GPU_CMD_VERTEX_EOL;
}

#define QUADSTRIP_COUNT(count) (((count) - 2) * 2)
static GL_NO_INLINE void genQuadStrip(Vertex* output, GLuint count) {
    Vertex* dst = output + QUADSTRIP_COUNT(count) - 1;
    Vertex* src = output + count;//(count - 1);

    for (; count > 2; count -= 2) {
        // Have to copy because of src/dst overlapping on first quad
		Vertex src1 = src[-1], src2 = src[-2], src3 = src[-3], src4 = src[-4];

        *dst   = src3;
        (*dst--).flags = GPU_CMD_VERTEX_EOL;
        *dst-- = src4;
        *dst-- = src1;
        *dst-- = src2;
        src -= 2;
    }
}

#define TRIFAN_COUNT(count) (((count) - 2) * 3)
static GL_NO_INLINE void genTriangleFan(Vertex* output, GLuint count) {
    Vertex* dst = output + TRIFAN_COUNT(count) - 1;
    Vertex* src = output + count - 1;

    // Triangles generated as {first vertex, prior vertex, current vertex}
    // e.g. {v1, v2, v3, v4} produces {v1, v2, v3}, {v1, v3, v4}
    for (; count > 2; count--) {
        *dst   = *src--;
        (*dst--).flags = GPU_CMD_VERTEX_EOL;
        *dst-- = *src;
        *dst-- = *output;
    }
}

#define POINTS_COUNT(count) ((count) * 4)
static GL_NO_INLINE void genPoints(Vertex* output, GLuint count) {
    Vertex* dst = output + POINTS_COUNT(count) - 1;
    Vertex* src = output + count - 1;
    float half_size = HALF_POINT_SIZE;

    // Expands v to { v + (S/2,-S/2), v + (S/2,S/2), v + (-S/2,-S/2), (-S/2,S/2) }
    for (; count > 0; count--, src--) {
        *dst = *src;
        dst->flags = GPU_CMD_VERTEX_EOL;
        dst->xyz[0] -= half_size; dst->xyz[1] += half_size;
        dst--;

        *dst = *src;
        dst->xyz[0] += half_size; dst->xyz[1] += half_size;
        dst--;

        *dst = *src;
        dst->xyz[0] -= half_size; dst->xyz[1] -= half_size;
        dst--;

        *dst = *src;
        dst->xyz[0] += half_size; dst->xyz[1] -= half_size;
        dst--;
    }
}

// Heavily based on the pvrline example by jnmartin84
// Which is based on https://devcry.heiho.net/html/2017/20170820-opengl-line-drawing.html
static Vertex* draw_line(Vertex* dst, Vertex* v1, Vertex* v2) {
    Vertex ov1 = *v1;
    Vertex ov2 = *v2;
    // TODO don't copy unless dst might overlap v1/v2 

	// Essentially "expands" a line into a quad by
    // 1) Calculating normal of the line from v1 to v2
	// 2) Scaling normal by the line width
	// 3) Offseting the endpoints wrt the scaled normal
    float dx = ov2.xyz[0] - ov1.xyz[0];
    float dy = ov2.xyz[1] - ov1.xyz[1];

    float inverse_mag = fast_rsqrt((dx*dx) + (dy*dy)) * HALF_LINE_WIDTH;
    float nx = -dy * inverse_mag;
    float ny =  dx * inverse_mag;

    *dst = ov2;
    dst->flags = GPU_CMD_VERTEX_EOL;
    dst->xyz[0] -= nx;
    dst->xyz[1] -= ny;
    dst--;

    *dst = ov1;
    dst->xyz[0] -= nx;
    dst->xyz[1] -= ny;
    dst--;

    *dst = ov2;
    dst->xyz[0] += nx;
    dst->xyz[1] += ny;
    dst--;

    *dst = ov1;
    dst->xyz[0] += nx;
    dst->xyz[1] += ny;
    dst--;

    return dst;
}

#define LINES_COUNT(count) (((count) / 2) * 4)
static GL_NO_INLINE void genLines(Vertex* output, GLuint count) {
    Vertex* dst = output + LINES_COUNT(count) - 1;
    Vertex* src = output + count - 1;

    // Draws line using two vertices
    for (; count >= 2; count -= 2, src -= 2) {
        dst = draw_line(dst, src, src - 1);
    }
}

#define LINE_STRIP_COUNT(count) (((count) - 1) * 4)
static GL_NO_INLINE void genLineStrip(Vertex* output, GLuint count) {
    Vertex* dst = output + LINE_STRIP_COUNT(count) - 1;
    Vertex* src = output + count - 1;

    // Draws line using current and prior vertex
    for (; count > 1; count--, src--) {
        dst = draw_line(dst, src, src - 1);
    }
}

#define LINE_LOOP_COUNT(count) ((count) * 4)
static GL_NO_INLINE void genLineLoop(Vertex* output, GLuint count) {
    Vertex* dst = output + LINE_LOOP_COUNT(count) - 1;
    Vertex* src = output + count - 1;
	Vertex last = *src, first = *output;
	
    // Draws line using current and prior vertex
    for (; count > 1; count--, src--) {
        dst = draw_line(dst, src, src - 1);
    }

    // Connect first and last vertex
	draw_line(dst, &first, &last);
}

static void _readPositionData(const GLuint first, const GLuint count, Vertex* it) {
    const ReadAttributeFunc func = ATTRIB_LIST.vertex_func;
    const GLsizei vstride = ATTRIB_LIST.vertex.stride;
    const GLubyte* vptr = ((GLubyte*) ATTRIB_LIST.vertex.ptr + (first * vstride));

    ITERATE(count) {
        PREFETCH(vptr + vstride);
        func(vptr, (GLubyte*) it);
        it->flags = GPU_CMD_VERTEX;

        vptr += vstride;
        ++it;
    }
}

static void _readUVData(const GLuint first, const GLuint count, Vertex* it) {
    const ReadAttributeFunc func = ATTRIB_LIST.uv_func;
    const GLsizei uvstride = ATTRIB_LIST.uv.stride;
    const GLubyte* uvptr = ((GLubyte*) ATTRIB_LIST.uv.ptr + (first * uvstride));

    ITERATE(count) {
        PREFETCH(uvptr + uvstride);

        func(uvptr, (GLubyte*) it->uv);
        uvptr += uvstride;
        ++it;
    }
}

static void _readSTData(const GLuint first, const GLuint count, VertexExtra* it) {
    const ReadAttributeFunc func = ATTRIB_LIST.st_func;
    const GLsizei ststride = ATTRIB_LIST.st.stride;
    const GLubyte* stptr = ((GLubyte*) ATTRIB_LIST.st.ptr + (first * ststride));

    ITERATE(count) {
        PREFETCH(stptr + ststride);
        func(stptr, (GLubyte*) it->st);
        stptr += ststride;
        ++it;
    }
}

static void _readNormalData(const GLuint first, const GLuint count, VertexExtra* it) {
    const ReadAttributeFunc func = ATTRIB_LIST.normal_func;
    const GLsizei nstride = ATTRIB_LIST.normal.stride;
    const GLubyte* nptr = ((GLubyte*) ATTRIB_LIST.normal.ptr + (first * nstride));

    ITERATE(count) {
        func(nptr, (GLubyte*) it->nxyz);
        nptr += nstride;

        if(_glIsNormalizeEnabled()) {
            GLfloat* n = (GLfloat*) it->nxyz;
            float temp = n[0] * n[0] + n[1] * n[1] + n[2] * n[2];

            float ilength = MATH_fsrra(temp);
            n[0] *= ilength;
            n[1] *= ilength;
            n[2] *= ilength;
        }

        ++it;
    }
}

static void _readDiffuseData(const GLuint first, const GLuint count, Vertex* it) {
    const ReadAttributeFunc func = ATTRIB_LIST.colour_func;
    const GLuint cstride = ATTRIB_LIST.colour.stride;
    const GLubyte* cptr = ((GLubyte*) ATTRIB_LIST.colour.ptr) + (first * cstride);

    ITERATE(count) {
        PREFETCH(cptr + cstride);
        func(cptr, it->bgra);
        cptr += cstride;
        ++it;
    }
}

static void generateElements(
        SubmissionTarget* target, const GLsizei first, const GLuint count,
        const GLubyte* indices, const GLenum type) {

    const GLsizei istride = index_size(type);
    const IndexParseFunc IndexFunc = _calcParseIndexFunc(type);

    GLubyte* xyz;
    GLubyte* uv;
    GLubyte* bgra;
    GLubyte* st;
    GLubyte* nxyz;

    Vertex* output = _glSubmissionTargetStart(target);
    VertexExtra* ve = aligned_vector_at(target->extras, 0);

    float pos[3], w = 1.0f;
    uint32_t i = first;
    uint32_t idx = 0;

    const ReadAttributeFunc pos_func = ATTRIB_LIST.vertex_func;
    const GLsizei vstride = ATTRIB_LIST.vertex.stride;

    const ReadAttributeFunc uv_func = ATTRIB_LIST.uv_func;
    const GLuint uvstride = ATTRIB_LIST.uv.stride;

    const ReadAttributeFunc st_func = ATTRIB_LIST.st_func;
    const GLuint ststride = ATTRIB_LIST.st.stride;

    const ReadAttributeFunc diffuse_func = ATTRIB_LIST.colour_func;
    const GLuint dstride = ATTRIB_LIST.colour.stride;

    const ReadAttributeFunc normal_func = ATTRIB_LIST.normal_func;
    const GLuint nstride = ATTRIB_LIST.normal.stride;

    for(; i < first + count; ++i) {
        idx = IndexFunc(indices + (i * istride));

        xyz = (GLubyte*) ATTRIB_LIST.vertex.ptr + (idx * vstride);
        uv = (GLubyte*) ATTRIB_LIST.uv.ptr + (idx * uvstride);
        bgra = (GLubyte*) ATTRIB_LIST.colour.ptr + (idx * dstride);
        st = (GLubyte*) ATTRIB_LIST.st.ptr + (idx * ststride);
        nxyz = (GLubyte*) ATTRIB_LIST.normal.ptr + (idx * nstride);

        pos_func(xyz, (GLubyte*) output);
        uv_func(uv, (GLubyte*) output->uv);
        diffuse_func(bgra, output->bgra);
        st_func(st, (GLubyte*) ve->st);
        normal_func(nxyz, (GLubyte*) ve->nxyz);

        output->flags = GPU_CMD_VERTEX;
        ++output;
        ++ve;
    }
}

typedef struct {
    float x, y, z;
} Float3;

typedef struct {
    float u, v;
} Float2;

static const Float3 F3Z = {0.0f, 0.0f, 1.0f};
static const Float2 F2ZERO = {0.0f, 0.0f};

static void generateElementsFastPath(
        SubmissionTarget* target, const GLsizei first, const GLuint count,
        const GLubyte* indices, const GLenum type) {

    Vertex* start = _glSubmissionTargetStart(target);

    const GLuint vstride = ATTRIB_LIST.vertex.stride;
    const GLuint uvstride = ATTRIB_LIST.uv.stride;
    const GLuint ststride = ATTRIB_LIST.st.stride;
    const GLuint dstride = ATTRIB_LIST.colour.stride;
    const GLuint nstride = ATTRIB_LIST.normal.stride;

    const GLsizei istride = index_size(type);
    const IndexParseFunc IndexFunc = _calcParseIndexFunc(type);

    /* Copy the pos, uv and color directly in one go */
    const GLubyte* pos = (ATTRIB_LIST.enabled & VERTEX_ENABLED_FLAG) ? ATTRIB_LIST.vertex.ptr : NULL;
    const GLubyte* uv  = (ATTRIB_LIST.enabled & UV_ENABLED_FLAG) ? ATTRIB_LIST.uv.ptr : NULL;
    const GLubyte* col = (ATTRIB_LIST.enabled & DIFFUSE_ENABLED_FLAG) ? ATTRIB_LIST.colour.ptr : NULL;
    const GLubyte* st  = (ATTRIB_LIST.enabled & ST_ENABLED_FLAG) ? ATTRIB_LIST.st.ptr : NULL;
    const GLubyte* n   = (ATTRIB_LIST.enabled & NORMAL_ENABLED_FLAG) ? ATTRIB_LIST.normal.ptr : NULL;

    VertexExtra* ve = aligned_vector_at(target->extras, 0);
    Vertex* it = start;

    if(!pos) {
        return;
    }

    for(GLuint i = first; i < first + count; ++i) {
        GLuint idx = IndexFunc(indices + (i * istride));

        it->flags = GPU_CMD_VERTEX;

        pos = (GLubyte*) ATTRIB_LIST.vertex.ptr + (idx * vstride);
        TransformVertex(((float*) pos)[0], ((float*) pos)[1], ((float*) pos)[2], 1.0f, it->xyz, &it->w);

        if(uv) {
            uv = (GLubyte*) ATTRIB_LIST.uv.ptr + (idx * uvstride);
            MEMCPY4(it->uv, uv, sizeof(float) * 2);
        } else {
            *((Float2*) it->uv) = F2ZERO;
        }

        if(col) {
            col = (GLubyte*) ATTRIB_LIST.colour.ptr + (idx * dstride);
            MEMCPY4(it->bgra, col, sizeof(uint32_t));
        } else {
            *((uint32_t*) it->bgra) = ~0;
        }

        if(st) {
            st = (GLubyte*) ATTRIB_LIST.st.ptr + (idx * ststride);
            MEMCPY4(ve->st, st, sizeof(float) * 2);
        } else {
            *((Float2*) ve->st) = F2ZERO;
        }

        if(n) {
            n = (GLubyte*) ATTRIB_LIST.normal.ptr + (idx * nstride);
            MEMCPY4(ve->nxyz, n, sizeof(float) * 3);
        } else {
            *((Float3*) ve->nxyz) = F3Z;
        }

        it++;
        ve++;
    }
}

#define likely(x)      __builtin_expect(!!(x), 1)

#define POLYMODE ALL
#define PROCESS_VERTEX_FLAGS(it, i) { \
    (it)->flags = GPU_CMD_VERTEX; \
}

#include "draw_fastpath.inc"
#undef PROCESS_VERTEX_FLAGS
#undef POLYMODE

#define POLYMODE QUADS
#define PROCESS_VERTEX_FLAGS(it, i) { \
    it->flags = GPU_CMD_VERTEX; \
    if(((i + 1) % 4) == 0) { \
        Vertex t = *it; \
        *it = *(it - 1); \
        *(it - 1) = t; \
        it->flags = GPU_CMD_VERTEX_EOL; \
    } \
}

#include "draw_fastpath.inc"
#undef PROCESS_VERTEX_FLAGS
#undef POLYMODE

#define POLYMODE TRIS
#define PROCESS_VERTEX_FLAGS(it, i) { \
    it->flags = ((i + 1) % 3 == 0) ? GPU_CMD_VERTEX_EOL : GPU_CMD_VERTEX; \
}
#include "draw_fastpath.inc"
#undef PROCESS_VERTEX_FLAGS
#undef POLYMODE

static void generateArrays(SubmissionTarget* target, const GLsizei first, const GLuint count) {
    Vertex* start = _glSubmissionTargetStart(target);
    VertexExtra* ve = aligned_vector_at(target->extras, 0);

    _readPositionData(first, count, start);
    _readDiffuseData(first, count, start);
    _readUVData(first, count, start);
    _readNormalData(first, count, ve);
    _readSTData(first, count, ve);
}

static void generate(SubmissionTarget* target, const GLenum mode, const GLsizei first, const GLuint count,
        const GLubyte* indices, const GLenum type) {
    /* Read from the client buffers and generate an array of ClipVertices */
    TRACE();

    if(ATTRIB_LIST.fast_path) {
        if(indices) {
            generateElementsFastPath(target, first, count, indices, type);
        } else {
            switch(mode) {
                case GL_QUADS:
                    generateArraysFastPath_QUADS(target, first, count);
                    return;  // Don't need to do any more processing
                case GL_TRIANGLES:
                    generateArraysFastPath_TRIS(target, first, count);
                    return; // Don't need to do any more processing
                default:
                    generateArraysFastPath_ALL(target, first, count);
            }
        }
    } else {
        if(indices) {
            generateElements(target, first, count, indices, type);
        } else {
            generateArrays(target, first, count);
        }
    }

    Vertex* it = _glSubmissionTargetStart(target);
    // Drawing arrays
    switch(mode) {
    case GL_TRIANGLES:
        genTriangles(it, count);
        break;
    case GL_QUADS:
        genQuads(it, count);
        break;
    case GL_TRIANGLE_STRIP:
        genTriangleStrip(it, count);
        break;

    case GL_QUAD_STRIP:
        genQuadStrip(it, count);
        break;
    case GL_TRIANGLE_FAN:
        genTriangleFan(it, count);
        break;

    case GL_POINTS:
        genPoints(it, count);
        break;
    case GL_LINES:
        genLines(it, count);
        break;
    case GL_LINE_STRIP:
        genLineStrip(it, count);
        break;
    case GL_LINE_LOOP:
        genLineLoop(it, count);
        break;
    default:
        gl_assert(0 && "Not Implemented");
    }
}

GL_FORCE_INLINE int _calc_pvr_face_culling() {
    if(!_glIsCullingEnabled()) {
        return GPU_CULLING_SMALL;
    } else {
        if(_glGetCullFace() == GL_BACK) {
            return (_glGetFrontFace() == GL_CW) ? GPU_CULLING_CCW : GPU_CULLING_CW;
        } else {
            return (_glGetFrontFace() == GL_CCW) ? GPU_CULLING_CCW : GPU_CULLING_CW;
        }
    }
}

GL_FORCE_INLINE int _calc_pvr_depth_test() {
    if(!_glIsDepthTestEnabled()) {
        return GPU_DEPTHCMP_ALWAYS;
    }

    switch(_glGetDepthFunc()) {
        case GL_NEVER:
            return GPU_DEPTHCMP_NEVER;
        case GL_LESS:
            return GPU_DEPTHCMP_GREATER;
        case GL_EQUAL:
            return GPU_DEPTHCMP_EQUAL;
        case GL_LEQUAL:
            return GPU_DEPTHCMP_GEQUAL;
        case GL_GREATER:
            return GPU_DEPTHCMP_LESS;
        case GL_NOTEQUAL:
            return GPU_DEPTHCMP_NOTEQUAL;
        case GL_GEQUAL:
            return GPU_DEPTHCMP_LEQUAL;
        break;
        case GL_ALWAYS:
        default:
            return GPU_DEPTHCMP_ALWAYS;
    }
}

GL_FORCE_INLINE void apply_poly_header(PolyHeader* header, GLboolean multiTextureHeader, PolyList* activePolyList, GLshort textureUnit) {
    TRACE();

    // Compile the header
    PolyContext ctx;
    memset(&ctx, 0, sizeof(PolyContext));

    ctx.list_type = activePolyList->list_type;
    ctx.fmt.color = GPU_CLRFMT_ARGBPACKED;
    ctx.fmt.uv = GPU_UVFMT_32BIT;
    ctx.gen.color_clamp = GPU_CLRCLAMP_DISABLE;

    ctx.gen.culling = _calc_pvr_face_culling();
    ctx.depth.comparison = _calc_pvr_depth_test();
    ctx.depth.write = _glIsDepthWriteEnabled() ? GPU_DEPTHWRITE_ENABLE : GPU_DEPTHWRITE_DISABLE;

    ctx.gen.shading = (_glGetShadeModel() == GL_SMOOTH) ? GPU_SHADE_GOURAUD : GPU_SHADE_FLAT;

    if(_glIsScissorTestEnabled()) {
        ctx.gen.clip_mode = GPU_USERCLIP_INSIDE;
    } else {
        ctx.gen.clip_mode = GPU_USERCLIP_DISABLE;
    }

    if(_glIsFogEnabled()) {
        ctx.gen.fog_type = GPU_FOG_TABLE;
    } else {
        ctx.gen.fog_type = GPU_FOG_DISABLE;
    }

    if(_glIsBlendingEnabled() || _glIsAlphaTestEnabled()) {
        ctx.gen.alpha = GPU_ALPHA_ENABLE;
    } else {
        ctx.gen.alpha = GPU_ALPHA_DISABLE;
    }

    if(ctx.list_type == GPU_LIST_OP_POLY) {
        /* Opaque polys are always one/zero */
        ctx.blend.src = GPU_BLEND_ONE;
        ctx.blend.dst = GPU_BLEND_ZERO;
    } else if(ctx.list_type == GPU_LIST_PT_POLY) {
        /* Punch-through polys require fixed blending and depth modes */
        ctx.blend.src = GPU_BLEND_SRCALPHA;
        ctx.blend.dst = GPU_BLEND_INVSRCALPHA;
        ctx.depth.comparison = GPU_DEPTHCMP_LEQUAL;
    } else {
        ctx.blend.src = _glGetGpuBlendSrcFactor();
        ctx.blend.dst = _glGetGpuBlendDstFactor();

        if(ctx.list_type == GPU_LIST_TR_POLY && AUTOSORT_ENABLED) {
            /* Autosort mode requires this mode for transparent polys */
            ctx.depth.comparison = GPU_DEPTHCMP_GEQUAL;
        }
    }

    _glUpdatePVRTextureContext(&ctx, textureUnit);

    if(multiTextureHeader) {
        gl_assert(ctx.list_type == GPU_LIST_TR_POLY);

        ctx.gen.alpha = GPU_ALPHA_ENABLE;
        ctx.txr.alpha = GPU_TXRALPHA_ENABLE;
        ctx.blend.src = GPU_BLEND_ZERO;
        ctx.blend.dst = GPU_BLEND_DESTCOLOR;
        ctx.depth.comparison = GPU_DEPTHCMP_EQUAL;
    }

    CompilePolyHeader(header, &ctx);

    /* Force bits 18 and 19 on to switch to 6 triangle strips */
    header->cmd |= 0xC0000;

    /* Post-process the vertex list */
    /*
     * This is currently unnecessary. aligned_vector memsets the allocated objects
     * to zero, and we don't touch oargb, also, we don't *enable* oargb yet in the
     * pvr header so it should be ignored anyway. If this ever becomes a problem,
     * uncomment this.
    ClipVertex* vout = output;
    const ClipVertex* end = output + count;
    while(vout < end) {
        vout->oargb = 0;
    }
    */
}

#define DEBUG_CLIPPING 0


static AlignedVector VERTEX_EXTRAS;
static SubmissionTarget SUBMISSION_TARGET;


void _glInitSubmissionTarget() {
    SubmissionTarget* target = &SUBMISSION_TARGET;

    target->extras = NULL;
    target->count = 0;
    target->output = NULL;
    target->header_offset = target->start_offset = 0;

    aligned_vector_init(&VERTEX_EXTRAS, sizeof(VertexExtra));
    target->extras = &VERTEX_EXTRAS;
}

GL_FORCE_INLINE GLuint calcFinalVertices(GLenum mode, GLuint count) {
    switch (mode) {
        case GL_POINTS:
            return POINTS_COUNT(count);
        case GL_LINE_LOOP:
            return LINE_LOOP_COUNT(count);
        case GL_LINE_STRIP:
            return LINE_STRIP_COUNT(count);
        case GL_LINES:
            return LINES_COUNT(count);
        case GL_TRIANGLE_FAN:
            return TRIFAN_COUNT(count);
        case GL_QUAD_STRIP:
            return QUADSTRIP_COUNT(count);
    }
    return count;
}

GL_FORCE_INLINE void submitVertices(GLenum mode, GLsizei first, GLuint count, GLenum type, const GLvoid* indices) {
    SubmissionTarget* const target = &SUBMISSION_TARGET;
    AlignedVector* const extras = target->extras;

    TRACE();

    /* Do nothing if vertices aren't enabled */
    if(!(ATTRIB_LIST.enabled & VERTEX_ENABLED_FLAG)) return;
    if(ATTRIB_LIST.dirty) _glUpdateAttributes();

    /* No vertices? Do nothing */
    if(!count) return;

    /* Polygons are treated as triangle fans, the only time this would be a
     * problem is if we supported glPolygonMode(..., GL_LINE) but we don't.
     * We optimise the triangle and quad cases.
     */
    if(mode == GL_POLYGON) {
        switch(count) {
            case 2:
                mode = GL_LINES;
            break;
            case 3:
                mode = GL_TRIANGLES;
            break;
            case 4:
                mode = GL_QUADS;
            break;
            default:
                mode = GL_TRIANGLE_FAN;
        }
    }

    target->output = _glActivePolyList();
    gl_assert(target->output);
    gl_assert(extras);

    uint32_t vector_size = aligned_vector_size(&target->output->vector);

    GLboolean header_required = (vector_size == 0) || _glGPUStateIsDirty();

    target->count = calcFinalVertices(mode, count);
    target->header_offset = vector_size;
    target->start_offset = target->header_offset + (header_required ? 1 : 0);

    gl_assert(target->start_offset >= target->header_offset);
    gl_assert(target->count);

    /* Make sure we have enough room for all the "extra" data */
    aligned_vector_resize(extras, target->count);

    /* Make room for the vertices and header */
    aligned_vector_extend(&target->output->vector, target->count + (header_required));

    if(header_required) {
        apply_poly_header(_glSubmissionTargetHeader(target), GL_FALSE, target->output, 0);
        _glGPUStateMarkClean();
    }

    _glTnlLoadMatrix();

    generate(target, mode, first, count, (GLubyte*) indices, type);

    _glTnlApplyEffects(target);

    // /*
    //    Now, if multitexturing is enabled, we want to send exactly the same vertices again, except:
    //    - We want to enable blending, and send them to the TR list
    //    - We want to set the depth func to GL_EQUAL
    //    - We want to set the second texture ID
    //    - We want to set the uv coordinates to the passed st ones
    // */

    // if(!TEXTURES_ENABLED[1]) {
    //     /* Multitexture actively disabled */
    //     return;
    // }

    // TextureObject* texture1 = _glGetTexture1();

    // /* Multitexture implicitly disabled */
    // if(!texture1 || ((ATTRIB_LIST.enabled & ST_ENABLED_FLAG) != ST_ENABLED_FLAG)) {
    //     /* Multitexture actively disabled */
    //     return;
    // }

    // /* Push back a copy of the list to the transparent poly list, including the header
    //     (hence the + 1)
    // */
    // Vertex* vertex = aligned_vector_push_back(
    //     &_glTransparentPolyList()->vector, (Vertex*) _glSubmissionTargetHeader(target), target->count + 1
    // );

    // gl_assert(vertex);

    // PolyHeader* mtHeader = (PolyHeader*) vertex++;
    // /* Send the buffer again to the transparent list */
    // apply_poly_header(mtHeader, GL_TRUE, _glTransparentPolyList(), 1);

    // /* Replace the UV coordinates with the ST ones */
    // VertexExtra* ve = aligned_vector_at(target->extras, 0);
    // ITERATE(target->count) {
    //     vertex->uv[0] = ve->st[0];
    //     vertex->uv[1] = ve->st[1];
    //     ++vertex;
    //     ++ve;
    // }
}

void APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) {
    TRACE();

    if(_glCheckImmediateModeInactive(__func__)) {
        return;
    }

    submitVertices(mode, 0, count, type, indices);
}

void APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    TRACE();

    if(_glCheckImmediateModeInactive(__func__)) {
        return;
    }

    submitVertices(mode, first, count, GL_UNSIGNED_INT, NULL);
}

GLuint _glGetActiveClientTexture() {
    return ACTIVE_CLIENT_TEXTURE;
}

void APIENTRY glClientActiveTextureARB(GLenum texture) {
    TRACE();

    if(texture < GL_TEXTURE0_ARB || texture > GL_TEXTURE0_ARB + MAX_GLDC_TEXTURE_UNITS) {
        _glKosThrowError(GL_INVALID_ENUM, __func__);
        return;
    }

    ACTIVE_CLIENT_TEXTURE = (texture == GL_TEXTURE1_ARB) ? 1 : 0;
}
