/* THIS FILE IS INCLUDED BY draw.c TO AVOID CODE DUPLICATION. IT'S AN UGLY HACK */

#define FUNC_NAME(mode) static void generateArraysFastPath##_##mode(SubmissionTarget* target, const GLsizei first, const GLuint count)
#define MAKE_FUNC(mode) FUNC_NAME(mode)

MAKE_FUNC(POLYMODE)
{
    const Vertex* const start = _glSubmissionTargetStart(target);
    const VertexExtra* const ve_start = aligned_vector_at(target->extras, 0);

    const GLuint vstride = ATTRIB_POINTERS.vertex.stride;
    GLuint uvstride = ATTRIB_POINTERS.uv.stride;
    GLuint ststride = ATTRIB_POINTERS.st.stride;
    GLuint dstride = ATTRIB_POINTERS.colour.stride;
    GLuint nstride = ATTRIB_POINTERS.normal.stride;

    const GLubyte* pos = (ENABLED_VERTEX_ATTRIBUTES & VERTEX_ENABLED_FLAG) ? ATTRIB_POINTERS.vertex.ptr + (first * vstride) : NULL;
    const GLubyte* uv = (ENABLED_VERTEX_ATTRIBUTES & UV_ENABLED_FLAG) ? ATTRIB_POINTERS.uv.ptr + (first * uvstride) : NULL;
    const GLubyte* col = (ENABLED_VERTEX_ATTRIBUTES & DIFFUSE_ENABLED_FLAG) ? ATTRIB_POINTERS.colour.ptr + (first * dstride) : NULL;
    const GLubyte* st = (ENABLED_VERTEX_ATTRIBUTES & ST_ENABLED_FLAG) ? ATTRIB_POINTERS.st.ptr + (first * ststride) : NULL;
    const GLubyte* n = (ENABLED_VERTEX_ATTRIBUTES & NORMAL_ENABLED_FLAG) ? ATTRIB_POINTERS.normal.ptr + (first * nstride) : NULL;

    const float w = 1.0f;

    if(!pos) {
        /* If we don't have vertices, do nothing */
        return;
    }

    if(!col) {
        col = (GLubyte*) &U4ONE;
        dstride = 0;
    }

    if(!uv) {
        uv = (GLubyte*) &F2ZERO;
        uvstride = 0;
    }

    if(!st) {
        st = (GLubyte*) &F2ZERO;
        ststride = 0;
    }

    if(!n) {
        n = (GLubyte*) &F3Z;
        nstride = 0;
    }

    VertexExtra* ve = (VertexExtra*) ve_start;
    Vertex* it = (Vertex*) start;

    for(int_fast32_t i = 0; i < count; ++i) {
        TransformVertex((const float*) pos, &w, it->xyz, &it->w);
        pos += vstride;
        PREFETCH(pos);

        *((Float2*) it->uv) = *((Float2*) uv);
        uv += uvstride;
        PREFETCH(uv);

        *((uint32_t*) it->bgra) = *((uint32_t*) col);
        col += dstride;
        PREFETCH(col);

        *((Float2*) ve->st) = *((Float2*) st);
        st += ststride;
        PREFETCH(st);

        *((Float3*) ve->nxyz) = *((Float3*) n);
        n += nstride;
        PREFETCH(n);

        PROCESS_VERTEX_FLAGS(it, i);

        ++it;
        ++ve;
    }
}
