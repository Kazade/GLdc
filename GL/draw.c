#include <stdio.h>
#include <string.h>

#include "../include/gl.h"
#include "../include/glext.h"
#include "private.h"

typedef struct {
    const void* ptr;
    GLenum type;
    GLsizei stride;
    GLint size;
} AttribPointer;


static AttribPointer VERTEX_POINTER;
static AttribPointer UV_POINTER;
static AttribPointer ST_POINTER;
static AttribPointer NORMAL_POINTER;
static AttribPointer DIFFUSE_POINTER;

#define VERTEX_ENABLED_FLAG     (1 << 0)
#define UV_ENABLED_FLAG         (1 << 1)
#define ST_ENABLED_FLAG         (1 << 2)
#define DIFFUSE_ENABLED_FLAG    (1 << 3)
#define NORMAL_ENABLED_FLAG     (1 << 4)

static GLuint ENABLED_VERTEX_ATTRIBUTES = 0;
static GLubyte ACTIVE_CLIENT_TEXTURE = 0;

void initAttributePointers() {
    TRACE();

    VERTEX_POINTER.ptr = NULL;
    VERTEX_POINTER.stride = 0;
    VERTEX_POINTER.type = GL_FLOAT;
    VERTEX_POINTER.size = 4;

    DIFFUSE_POINTER.ptr = NULL;
    DIFFUSE_POINTER.stride = 0;
    DIFFUSE_POINTER.type = GL_FLOAT;
    DIFFUSE_POINTER.size = 4;

    UV_POINTER.ptr = NULL;
    UV_POINTER.stride = 0;
    UV_POINTER.type = GL_FLOAT;
    UV_POINTER.size = 4;

    ST_POINTER.ptr = NULL;
    ST_POINTER.stride = 0;
    ST_POINTER.type = GL_FLOAT;
    ST_POINTER.size = 4;

    NORMAL_POINTER.ptr = NULL;
    NORMAL_POINTER.stride = 0;
    NORMAL_POINTER.type = GL_FLOAT;
    NORMAL_POINTER.size = 3;
}

static GLuint byte_size(GLenum type) {
    switch(type) {
    case GL_BYTE: return sizeof(GLbyte);
    case GL_UNSIGNED_BYTE: return sizeof(GLubyte);
    case GL_SHORT: return sizeof(GLshort);
    case GL_UNSIGNED_SHORT: return sizeof(GLushort);
    case GL_INT: return sizeof(GLint);
    case GL_UNSIGNED_INT: return sizeof(GLuint);
    case GL_DOUBLE: return sizeof(GLdouble);
    case GL_FLOAT:
    default: return sizeof(GLfloat);
    }
}

static GLfloat transformVertexWithoutPerspectiveDivide(GLfloat* src, float* x, float* y, float* z) {
    register float __x __asm__("fr12") = (src[0]);
    register float __y __asm__("fr13") = (src[1]);
    register float __z __asm__("fr14") = (src[2]);
    register float __w __asm__("fr15");

    __asm__ __volatile__(
        "fldi1 fr15\n"
        "ftrv  xmtrx, fv12\n"
        : "=f" (__x), "=f" (__y), "=f" (__z)
        : "0" (__x), "1" (__y), "2" (__z)
    );

    *x = __x;
    *y = __y;
    *z = __z;

    return __w;
}

static void _parseColour(uint32* out, const GLubyte* in, GLint size, GLenum type) {
    switch(type) {
    case GL_BYTE: {
    case GL_UNSIGNED_BYTE:
        *out = in[3] << 24 | in[0] << 16 | in[1] << 8 | in[0];
    } break;
    case GL_SHORT:
    case GL_UNSIGNED_SHORT:
        /* FIXME!!!! */
    break;
    case GL_INT:
    case GL_UNSIGNED_INT:
        /* FIXME!!!! */
    break;
    case GL_FLOAT:
    case GL_DOUBLE:
    default: {
        const GLfloat* src = (GLfloat*) in;
        *out = PVR_PACK_COLOR(src[3], src[0], src[1], src[2]);
    } break;
    }
}

static void _parseFloats(GLfloat* out, const GLubyte* in, GLint size, GLenum type) {
    GLubyte i;

    switch(type) {
    case GL_SHORT: {
        GLshort* inp = (GLshort*) in;
        for(i = 0; i < size; ++i) {
            out[i] = (GLfloat) inp[i];
        }
    } break;
    case GL_INT: {
        GLint* inp = (GLint*) in;
        for(i = 0; i < size; ++i) {
            out[i] = (GLfloat) inp[i];
        }
    } break;
    case GL_FLOAT:
    case GL_DOUBLE:  /* Double == Float */
        default: {
            const GLfloat* ptr = (const GLfloat*) in;
            for(i = 0; i < size; ++i) out[i] = ptr[i];
        }
    }
}

static void _parseIndex(GLshort* out, const GLubyte* in, GLenum type) {
    switch(type) {
    case GL_UNSIGNED_BYTE:
        *out = (GLshort) *in;
    break;
    case GL_UNSIGNED_SHORT:
    default:
        *out = *((GLshort*) in);
    }
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



inline void transformToEyeSpace(GLfloat* point) {
    _matrixLoadModelView();
    mat_trans_single3_nodiv(point[0], point[1], point[2]);
}


inline void transformNormalToEyeSpace(GLfloat* normal) {
    _matrixLoadNormal();
    mat_trans_normal3(normal[0], normal[1], normal[2]);
}

/* If this has a value other than zero, it must be positive! */
#define NEAR_DEPTH 0.0001f

static void submitVertices(GLenum mode, GLsizei first, GLsizei count, GLenum type, const GLvoid* indices) {
    static GLfloat normal[3] = {0.0f, 0.0f, -1.0f};
    static GLfloat eye_P[3];
    static GLfloat eye_N[3];

    if(!(ENABLED_VERTEX_ATTRIBUTES & VERTEX_ENABLED_FLAG)) {
        return;
    }

    const GLsizei elements = (mode == GL_QUADS) ? 4 : (mode == GL_TRIANGLES) ? 3 : (mode == GL_LINES) ? 2 : count;

    // Point dest at the first new vertex to populate. This is the size of the container before extending,
    // with the additional space for the header.
    GLsizei start_of_output = activePolyList()->vector.size + 1;

    AlignedVector* list_vector = &activePolyList()->vector;

    // Make room for the element + the header
    PVRCommand* dst = (PVRCommand*) aligned_vector_extend(list_vector, count + 1);

    // Store a pointer to the header
    pvr_poly_hdr_t* hdr = (pvr_poly_hdr_t*) dst;

    dst++;

    // Compile
    pvr_poly_cxt_t cxt = *getPVRContext();
    cxt.list_type = activePolyList()->list_type;

    updatePVRTextureContext(&cxt, getTexture0());

    pvr_poly_compile(hdr, &cxt);

    GLubyte vstride = (VERTEX_POINTER.stride) ? VERTEX_POINTER.stride : VERTEX_POINTER.size * byte_size(VERTEX_POINTER.type);
    const GLubyte* vptr = VERTEX_POINTER.ptr;

    GLubyte cstride = (DIFFUSE_POINTER.stride) ? DIFFUSE_POINTER.stride : DIFFUSE_POINTER.size * byte_size(DIFFUSE_POINTER.type);
    const GLubyte* cptr = DIFFUSE_POINTER.ptr;

    GLubyte uvstride = (UV_POINTER.stride) ? UV_POINTER.stride : UV_POINTER.size * byte_size(UV_POINTER.type);
    const GLubyte* uvptr = UV_POINTER.ptr;

    GLubyte nstride = (NORMAL_POINTER.stride) ? NORMAL_POINTER.stride : NORMAL_POINTER.size * byte_size(NORMAL_POINTER.type);
    const GLubyte* nptr = NORMAL_POINTER.ptr;

    const GLubyte* indices_as_bytes = (GLubyte*) indices;

    GLboolean lighting_enabled = isLightingEnabled();

    GLushort i, last_vertex;
    GLshort rel; // Has to be signed as we drop below zero so we can re-enter the loop at 1.

    static AlignedVector w_coordinates;
    static GLboolean w_coordinates_initialized = GL_FALSE;
    if(!w_coordinates_initialized) {
        aligned_vector_init(&w_coordinates, sizeof(GLfloat));
        w_coordinates_initialized = GL_TRUE;
    }
    aligned_vector_resize(&w_coordinates, 0);

    static struct {
        pvr_vertex_t* vin[3];
        GLfloat w[3];
        GLubyte vcount;
    } Triangle;

    Triangle.vcount = 0;

    /* Loop 1. Calculate vertex colours, transform, but don't apply perspective division */
    for(rel = 0, i = first; i < count; ++i, ++rel) {       
        pvr_vertex_t* vertex = (pvr_vertex_t*) dst;
        vertex->u = vertex->v = 0.0f;
        vertex->argb = 0;
        vertex->oargb = 0;
        vertex->flags = PVR_CMD_VERTEX;

        last_vertex = ((i + 1) % elements) == 0;

        if(last_vertex) {
            vertex->flags = PVR_CMD_VERTEX_EOL;
        }

        GLshort idx = i;
        if(indices) {
            _parseIndex(&idx, &indices_as_bytes[byte_size(type) * i], type);
        }

        _parseFloats(&vertex->x, vptr + (idx * vstride), VERTEX_POINTER.size, VERTEX_POINTER.type);

        if(ENABLED_VERTEX_ATTRIBUTES & DIFFUSE_ENABLED_FLAG) {
            _parseColour(&vertex->argb, cptr + (idx * cstride), DIFFUSE_POINTER.size, DIFFUSE_POINTER.type);
        } else {
            /* Default to white if colours are disabled */
            vertex->argb = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);
        }

        if(ENABLED_VERTEX_ATTRIBUTES & UV_ENABLED_FLAG) {
            _parseFloats(&vertex->u, uvptr + (idx * uvstride), UV_POINTER.size, UV_POINTER.type);
        }

        if(ENABLED_VERTEX_ATTRIBUTES & NORMAL_ENABLED_FLAG) {
            _parseFloats(normal, nptr + (idx * nstride), NORMAL_POINTER.size, NORMAL_POINTER.type);
        } else {
            normal[0] = normal[1] = 0.0f;
            normal[2] = -1.0f;
        }

        if(lighting_enabled) {
            /* We ignore diffuse colour when lighting is enabled. If GL_COLOR_MATERIAL is enabled
             * then the lighting calculation should possibly take it into account */
            GLfloat contribution [] = {0.0f, 0.0f, 0.0f, 0.0f};
            GLfloat to_add [] = {0.0f, 0.0f, 0.0f, 0.0f};

            /* Transform the vertex and normal into eye-space */
            eye_P[0] = vertex->x;
            eye_P[1] = vertex->y;
            eye_P[2] = vertex->z;

            eye_N[0] = normal[0];
            eye_N[1] = normal[1];
            eye_N[2] = normal[2];

            transformToEyeSpace(eye_P);
            transformNormalToEyeSpace(eye_N);

            GLubyte j;
            for(j = 0; j < MAX_LIGHTS; ++j) {
                if(isLightEnabled(j)) {
                    calculateLightingContribution(j, eye_P, eye_N, to_add);

                    contribution[0] += to_add[0];
                    contribution[1] += to_add[1];
                    contribution[2] += to_add[2];
                    contribution[3] += to_add[3];
                }
            }

            vertex->argb = PVR_PACK_COLOR(contribution[3], contribution[0], contribution[1], contribution[2]);
        }

        _applyRenderMatrix(); /* Apply the Render Matrix Stack */

        /* Perform transformation without perspective division. Perspective divide will occur
         * per-triangle after clipping */
        GLfloat W = transformVertexWithoutPerspectiveDivide(&vertex->x, &vertex->x, &vertex->y, &vertex->z);
        aligned_vector_push_back(&w_coordinates, &W, 1);

        Triangle.w[Triangle.vcount] = W;
        Triangle.vin[Triangle.vcount] = vertex;
        Triangle.vcount++;
        if(Triangle.vcount == 3) {
            pvr_vertex_t clipped[4];

            /* OK we have a whole triangle, we may have to clip */
            TriangleClipResult tri_result = clipTriangleToNearZ(
                NEAR_DEPTH,
                (rel - 2),
                Triangle.vin[0],
                Triangle.vin[1],
                Triangle.vin[2],
                &clipped[0],
                &clipped[1],
                &clipped[2],
                &clipped[3]
            );

            /* The potential 4 new vertices that can be output by clipping the triangle. Initialized in the below branches */
            pvr_vertex_t* vout[4];

            if(tri_result == TRIANGLE_CLIP_RESULT_NO_CHANGE) {
                /* Nothing changed, we're fine */
            } else if(tri_result == TRIANGLE_CLIP_RESULT_DROP_TRIANGLE) {
                /* As we're dealing with triangle strips, we have the following situations:
                 * 1. This is the first trangle. We can drop the first vertex, reverse the other two, subsequent
                 *    triangles will be formed from them. If there are no remaining vertices (i == count - 1) then
                 *    we can drop all three.
                 * 2. This is the second+ triangle. We mark the second triangle vertex as the "last" vertex, then
                 *    push the second and third vertices again (reversed) to start the new triangle. If there are no
                 *    more vertices (i == count - 1) we can just drop the final vertex.
                 * By first triangle, it means that rel == 2 or dst - 3 is marked as a "last" vertex.
                 */

                /* Is this the first triangle in the strip? */
                GLboolean first_triangle = (rel == 2) || ((vertex - 3)->flags == PVR_CMD_VERTEX_EOL);
                if(first_triangle) {
                    vout[0] = vertex - 2;
                    vout[1] = vertex - 1;
                    vout[2] = vertex;

                    if(rel == (count - 1)) {
                        /* Lose all 3 vertices */
                        aligned_vector_resize(list_vector, list_vector->size - 3);
                        dst -= 3;
                        vertex = (pvr_vertex_t*) dst;

                        /* Next triangle is a new one */
                        Triangle.vcount = 0;
                    } else {
                        vout[0] = vout[2];
                        /* vout[1] = vout[1]; no-op, just here as a comment so things make a bit more sense */

                        /* Rewind dst by one as we just lost a vertex */
                        aligned_vector_resize(list_vector, list_vector->size - 1);
                        dst--;
                        vertex = (pvr_vertex_t*) dst;

                        /* Two vertices are populated for the current triangle now */
                        Triangle.vcount = 2;
                    }
                } else {
                    if(rel == (count - 1)) {
                        /* This is the last vertex in the strip and we're dropping this triangle so just drop a vertex*/
                        aligned_vector_resize(list_vector, list_vector->size - 1);
                        dst--;
                        vertex = (pvr_vertex_t*) dst;
                    } else {
                        /* This is a bit weird. We're dropping a triangle, but we have to add an additional vertex. This is because
                         * if this triangle is in the middle of the strip, and we drop the 3rd vertex then we break the following triangle.
                         * so what we do is end the triangle strip at vertex 2 of the current triangle, then re-add vertex 2 and vertex 3
                         * in reverse so that the next triangle works. This might seem wasteful but actually that triangle could be subsequently
                         * dropped entirely as it'll be the "first_triangle" next time around. */

                        /* Make room at the end of the vector */
                        aligned_vector_extend(list_vector, 1);

                        /* Deal with any realloc that just happened */
                        dst = aligned_vector_at(list_vector, rel);
                        vertex = (pvr_vertex_t*) dst;

                        /* Set up the output pointers */
                        vout[0] = vertex - 2;
                        vout[1] = vertex - 1;
                        vout[2] = vertex;
                        vout[3] = vertex + 3;

                        /* Mark second vertex as the end of the strip, duplicate the second vertex
                         * to create the start of the next strip
                         */
                        *vout[3] = *Triangle.vin[1];

                        vout[1]->flags = PVR_CMD_VERTEX_EOL;
                        vout[2]->flags = PVR_CMD_VERTEX;
                        vout[3]->flags = PVR_CMD_VERTEX;

                        dst = (PVRCommand*) vout[3];
                        vertex = vout[3];

                        /* Current triangle has two vertices */
                        Triangle.vcount = 2;
                    }
                }
            } else if(tri_result == TRIANGLE_CLIP_RESULT_ALTERED_VERTICES) {

            } else if(tri_result == TRIANGLE_CLIP_RESULT_ALTERED_AND_CREATED_VERTEX) {

            }

            /* Reset for the next triangle */
            Triangle.vin[0] = Triangle.vin[1];
            Triangle.vin[1] = Triangle.vin[2];
            Triangle.w[0] = Triangle.w[1];
            Triangle.w[1] = Triangle.w[2];
            Triangle.vcount = 2;
        }

        ++dst;
    }

    pvr_vertex_t* v = (pvr_vertex_t*) aligned_vector_at(list_vector, start_of_output);

    /* Loop 2: Perspective division */
    for(rel = 0, i = start_of_output; i < activePolyList()->vector.size; ++rel, ++i) {
        GLfloat* w = aligned_vector_at(&w_coordinates, rel);

        register float __x __asm__("fr12") = (v->x);
        register float __y __asm__("fr13") = (v->y);
        register float __z __asm__("fr14") = (v->z);
        register float __w __asm__("fr15") = (*w);

        __asm__ __volatile__(
            "fldi1 fr14\n" \
            "fdiv	 fr15, fr14\n" \
            "fmul	 fr14, fr12\n" \
            "fmul	 fr14, fr13\n" \
            : "=f" (__x), "=f" (__y), "=f" (__z)
            : "0" (__x), "1" (__y), "2" (__z)
        );

        v->x = __x;
        v->y = __y;
        v->z = __z;
        ++v;
    }
}

void APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) {
    TRACE();

    if(checkImmediateModeInactive(__func__)) {
        return;
    }

    submitVertices(mode, 0, count, type, indices);
}

void APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    TRACE();

    if(checkImmediateModeInactive(__func__)) {
        return;
    }

    submitVertices(mode, first, count, GL_UNSIGNED_SHORT, NULL);
}

void APIENTRY glEnableClientState(GLenum cap) {
    TRACE();

    switch(cap) {
    case GL_VERTEX_ARRAY:
        ENABLED_VERTEX_ATTRIBUTES |= VERTEX_ENABLED_FLAG;
    break;
    case GL_COLOR_ARRAY:
        ENABLED_VERTEX_ATTRIBUTES |= DIFFUSE_ENABLED_FLAG;
    break;
    case GL_NORMAL_ARRAY:
        ENABLED_VERTEX_ATTRIBUTES |= NORMAL_ENABLED_FLAG;
    break;
    case GL_TEXTURE_COORD_ARRAY:
        (ACTIVE_CLIENT_TEXTURE) ?
            (ENABLED_VERTEX_ATTRIBUTES |= ST_ENABLED_FLAG):
            (ENABLED_VERTEX_ATTRIBUTES |= UV_ENABLED_FLAG);
    break;
    default:
        _glKosThrowError(GL_INVALID_ENUM, "glEnableClientState");
    }
}

void APIENTRY glDisableClientState(GLenum cap) {
    TRACE();

    switch(cap) {
    case GL_VERTEX_ARRAY:
        ENABLED_VERTEX_ATTRIBUTES &= ~VERTEX_ENABLED_FLAG;
    break;
    case GL_COLOR_ARRAY:
        ENABLED_VERTEX_ATTRIBUTES &= ~DIFFUSE_ENABLED_FLAG;
    break;
    case GL_NORMAL_ARRAY:
        ENABLED_VERTEX_ATTRIBUTES &= ~NORMAL_ENABLED_FLAG;
    break;
    case GL_TEXTURE_COORD_ARRAY:
        (ACTIVE_CLIENT_TEXTURE) ?
            (ENABLED_VERTEX_ATTRIBUTES &= ~ST_ENABLED_FLAG):
            (ENABLED_VERTEX_ATTRIBUTES &= ~UV_ENABLED_FLAG);
    break;
    default:
        _glKosThrowError(GL_INVALID_ENUM, "glDisableClientState");
    }
}

void APIENTRY glClientActiveTextureARB(GLenum texture) {
    TRACE();

    if(texture < GL_TEXTURE0_ARB || texture > GL_TEXTURE0_ARB + MAX_TEXTURE_UNITS) {
        _glKosThrowError(GL_INVALID_ENUM, "glClientActiveTextureARB");
    }

    if(_glKosHasError()) {
        _glKosPrintError();
        return;
    }

    ACTIVE_CLIENT_TEXTURE = (texture == GL_TEXTURE1_ARB) ? 1 : 0;
}

void APIENTRY glTexCoordPointer(GLint size,  GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    AttribPointer* tointer = (ACTIVE_CLIENT_TEXTURE == 0) ? &UV_POINTER : &ST_POINTER;

    tointer->ptr = pointer;
    tointer->stride = stride;
    tointer->type = type;
    tointer->size = size;
}

void APIENTRY glVertexPointer(GLint size,  GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    VERTEX_POINTER.ptr = pointer;
    VERTEX_POINTER.stride = stride;
    VERTEX_POINTER.type = type;
    VERTEX_POINTER.size = size;
}

void APIENTRY glColorPointer(GLint size,  GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    DIFFUSE_POINTER.ptr = pointer;
    DIFFUSE_POINTER.stride = stride;
    DIFFUSE_POINTER.type = type;
    DIFFUSE_POINTER.size = size;
}

void APIENTRY glNormalPointer(GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    NORMAL_POINTER.ptr = pointer;
    NORMAL_POINTER.stride = stride;
    NORMAL_POINTER.type = type;
    NORMAL_POINTER.size = 3;
}
