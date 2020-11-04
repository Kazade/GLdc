#pragma once

#include "private.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_STACK 4

#define B000 0
#define B111 7
#define B100 4
#define B010 2
#define B001 1
#define B101 5
#define B011 3
#define B110 6


typedef struct {
    /* Remaining vertices in the source list */
    int remaining;

    /* Current position in the source list */
    Vertex* src;

    /* Vertex to read from (this may not exist in the source list) */
    Vertex* active;

    /* Sliding window into the source view */
    Vertex* triangle[3];

    /* Stack of temporary vertices */
    Vertex stack[MAX_STACK];
    int8_t stack_idx;

    /* < 8. Bitmask of the last 3 vertices */
    uint8_t visibility;
    uint8_t triangle_count;
    uint8_t padding;
} ListIterator;

ListIterator* _glIteratorBegin(void* src, int n);

GL_FORCE_INLINE GLboolean isVertex(const Vertex* vertex) {
    assert(vertex);

    return (
        vertex->flags == PVR_CMD_VERTEX ||
        vertex->flags == PVR_CMD_VERTEX_EOL
    );
}

GL_FORCE_INLINE GLboolean isVisible(const Vertex* vertex) {
    TRACE();

    assert(vertex != NULL);
    return vertex->w > 0.0000f; // && vertex->xyz[2] >= -vertex->w;
}

ListIterator* _glIteratorNext(ListIterator* it);

#ifdef __cplusplus
}
#endif
