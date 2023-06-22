/* THIS FILE IS INCLUDED BY draw.c TO AVOID CODE DUPLICATION. IT'S AN UGLY HACK */

#define FUNC_NAME(mode) static void generateArraysFastPath##_##mode(SubmissionTarget* target, const GLsizei first, const GLuint count)
#define MAKE_FUNC(mode) FUNC_NAME(mode)

MAKE_FUNC(POLYMODE)
{
    static const float w = 1.0f;
    if(!(ENABLED_VERTEX_ATTRIBUTES & VERTEX_ENABLED_FLAG)) {
        /* If we don't have vertices, do nothing */
        return;
    }

    /* This is the best value we have. PROCESS_VERTEX_FLAGS needs to operate on quads and tris and so
       this need to be divisible by 4 and 3. Even though we should be able to go much higher than this
       and still be cache-local, trial and error says otherwise... */

#define BATCH_SIZE 60

    GLuint min = 0;
    GLuint stride;
    const GLubyte* ptr;
    Vertex* it;
    VertexExtra* ve;


    for(min = 0; min < count; min += BATCH_SIZE) {
        const Vertex* start = ((Vertex*) _glSubmissionTargetStart(target)) + min;
        const int_fast32_t loop = ((min + BATCH_SIZE) > count) ? count - min : BATCH_SIZE;
        const int offset = (first + min);

        stride = ATTRIB_POINTERS.uv.stride;
        ptr = (ENABLED_VERTEX_ATTRIBUTES & UV_ENABLED_FLAG) ? ATTRIB_POINTERS.uv.ptr + ((first + min) * stride) : NULL;
        it = (Vertex*) start;

        if(ptr) {
            PREFETCH(ptr);
            for(int_fast32_t i = 0; i < loop; ++i, ++it) {
                PREFETCH(ptr + stride);
                it->uv[0] = ((float*) ptr)[0];
                it->uv[1] = ((float*) ptr)[1];
                ptr += stride;
            }
        } else {
            for(int_fast32_t i = 0; i < loop; ++i, ++it) {
                it->uv[0] = 0;
                it->uv[1] = 0;
            }
        }

        stride = ATTRIB_POINTERS.colour.stride;
        ptr = (ENABLED_VERTEX_ATTRIBUTES & DIFFUSE_ENABLED_FLAG) ? ATTRIB_POINTERS.colour.ptr + (offset * stride) : NULL;
        it = (Vertex*) start;

        if(ptr) {
            PREFETCH(ptr);
            for(int_fast32_t i = 0; i < loop; ++i, ++it) {
                PREFETCH(ptr + stride);
                it->bgra[0] = ptr[0];
                it->bgra[1] = ptr[1];
                it->bgra[2] = ptr[2];
                it->bgra[3] = ptr[3];
                ptr += stride;
            }
        } else {
            for(int_fast32_t i = 0; i < loop; ++i, ++it) {
                *((uint32_t*) it->bgra) = ~0;
            }
        }

        stride = ATTRIB_POINTERS.vertex.stride;
        ptr = ATTRIB_POINTERS.vertex.ptr + (offset * stride);
        it = (Vertex*) start;

        PREFETCH(ptr);
        for(int_fast32_t i = 0; i < loop; ++i, ++it) {
            PREFETCH(ptr + stride);
            TransformVertex((const float*) ptr, &w, it->xyz, &it->w);
            PROCESS_VERTEX_FLAGS(it, min + i);
            ptr += stride;
        }

        start = aligned_vector_at(target->extras, min);

        stride = ATTRIB_POINTERS.st.stride;
        ptr = (ENABLED_VERTEX_ATTRIBUTES & ST_ENABLED_FLAG) ? ATTRIB_POINTERS.st.ptr + (offset * stride) : NULL;
        ve = (VertexExtra*) start;

        if(ptr) {
            PREFETCH(ptr);

            for(int_fast32_t i = 0; i < loop; ++i, ++ve) {
                PREFETCH(ptr + stride);
                ve->st[0] = ((float*) ptr)[0];
                ve->st[1] = ((float*) ptr)[1];
                ptr += stride;
            }
        } else {
            for(int_fast32_t i = 0; i < loop; ++i, ++ve) {
                ve->st[0] = 0;
                ve->st[1] = 0;
            }
        }

        stride = ATTRIB_POINTERS.normal.stride;
        ptr = (ENABLED_VERTEX_ATTRIBUTES & NORMAL_ENABLED_FLAG) ? ATTRIB_POINTERS.normal.ptr + (offset * stride) : NULL;
        ve = (VertexExtra*) start;

        if(ptr) {
            PREFETCH(ptr);

            for(int_fast32_t i = 0; i < loop; ++i, ++ve) {
                PREFETCH(ptr + stride);
                ve->nxyz[0] = ((float*) ptr)[0];
                ve->nxyz[1] = ((float*) ptr)[1];
                ve->nxyz[2] = ((float*) ptr)[2];
                ptr += stride;
            }
        } else {
            for(int_fast32_t i = 0; i < loop; ++i, ++ve) {
                ve->nxyz[0] = 0;
                ve->nxyz[1] = 0;
                ve->nxyz[2] = 0;
            }
        }
    }
}
