/* THIS FILE IS INCLUDED BY draw.c TO AVOID CODE DUPLICATION. IT'S AN UGLY HACK */

#define FUNC_NAME(mode) static void generateArraysFastPath##_##mode(SubmissionTarget* target, const GLsizei first, const GLuint count)
#define MAKE_FUNC(mode) FUNC_NAME(mode)

MAKE_FUNC(POLYMODE)
{
    if(!(ATTRIB_LIST.enabled & VERTEX_ENABLED_FLAG)) {
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


    for(min = 0; min < count; min += BATCH_SIZE) {
        const Vertex* start = ((Vertex*) _glSubmissionTargetStart(target)) + min;
        const int_fast32_t loop = ((min + BATCH_SIZE) > count) ? count - min : BATCH_SIZE;
        const int offset = (first + min);

        stride = ATTRIB_LIST.uv.stride;
        ptr = (ATTRIB_LIST.enabled & UV_ENABLED_FLAG) ? ATTRIB_LIST.uv.ptr + ((first + min) * stride) : NULL;
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
            float* uv = _glCurrentTexCoord0();
            for(int_fast32_t i = 0; i < loop; ++i, ++it) {
                it->uv[0] = uv[0];
                it->uv[1] = uv[1];
            }
        }

        stride = ATTRIB_LIST.colour.stride;
        ptr = (ATTRIB_LIST.enabled & COLOR_ENABLED_FLAG) ? ATTRIB_LIST.colour.ptr + (offset * stride) : NULL;
        it = (Vertex*) start;

        if(ptr) {
            PREFETCH(ptr);
            for(int_fast32_t i = 0; i < loop; ++i, ++it) {
                PREFETCH(ptr + stride);
                it->argb[0] = ((float*) ptr)[0];
                it->argb[1] = ((float*) ptr)[1];
                it->argb[2] = ((float*) ptr)[2];
                it->argb[3] = ((float*) ptr)[3];

                ptr += stride;
            }
        } else {
            float* color = _glCurrentColor();
            for(int_fast32_t i = 0; i < loop; ++i, ++it) {
                it->argb[0] = color[0];
                it->argb[1] = color[1];
                it->argb[2] = color[2];
                it->argb[3] = color[3];
            }
        }

        stride = ATTRIB_LIST.vertex.stride;
        ptr = ATTRIB_LIST.vertex.ptr + (offset * stride);
        it = (Vertex*) start;

        PREFETCH(ptr);
        for(int_fast32_t i = 0; i < loop; ++i, ++it) {
            PREFETCH(ptr + stride);
            TransformVertex(((float*) ptr)[0], ((float*) ptr)[1], ((float*) ptr)[2], 1.0f, it->xyz, &it->w);
            PROCESS_VERTEX_FLAGS(it, min + i);
            ptr += stride;
        }

        stride = ATTRIB_LIST.st.stride;
        ptr = (ATTRIB_LIST.enabled & ST_ENABLED_FLAG) ? ATTRIB_LIST.st.ptr + (offset * stride) : NULL;
        it = (Vertex*) start;

        if(ptr) {
            PREFETCH(ptr);

            for(int_fast32_t i = 0; i < loop; ++i, ++it) {
                PREFETCH(ptr + stride);
                it->st[0] = _glPackHalfFloat(((float*) ptr)[0]);
                it->st[1] = _glPackHalfFloat(((float*) ptr)[1]);
                ptr += stride;
            }
        } else {
            float* st = _glCurrentTexCoord0();

            for(int_fast32_t i = 0; i < loop; ++i, ++it) {
                it->st[0] = _glPackHalfFloat(st[0]);
                it->st[1] = _glPackHalfFloat(st[1]);
            }
        }

        stride = ATTRIB_LIST.normal.stride;
        ptr = (ATTRIB_LIST.enabled & NORMAL_ENABLED_FLAG) ? ATTRIB_LIST.normal.ptr + (offset * stride) : NULL;
        it = (Vertex*) start;

        if(ptr) {
            PREFETCH(ptr);

            for(int_fast32_t i = 0; i < loop; ++i, ++it) {
                PREFETCH(ptr + stride);
                it->nxyz = _glPackNormal((float*) ptr);
                ptr += stride;
            }
        } else {
            float* normal = _glCurrentNormal();
            for(int_fast32_t i = 0; i < loop; ++i, ++it) {
                it->nxyz = _glPackNormal(normal);
            }
        }
    }
}
