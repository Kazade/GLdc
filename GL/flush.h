#pragma once

#include "private.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_STACK 3

#define B000 0
#define B111 7
#define B100 4
#define B010 2
#define B001 1
#define B101 5
#define B011 3
#define B110 6


typedef struct {
    int remaining;

    /* Current position in the source list */
    Vertex* current;

    /* Vertex to read from (this may not exist in the source list) */
    Vertex* it;

    /* < 8. Bitmask of the last 3 vertices */
    uint8_t visibility;
    uint8_t triangle_count;
    Vertex* triangle[3];

    /* Stack of temporary vertices */
    Vertex stack[MAX_STACK];
    int8_t stack_idx;
} ListIterator;

inline ListIterator* _glIteratorBegin(void* src, int n) {
    ListIterator* it = (ListIterator*) malloc(sizeof(ListIterator));
    it->remaining = n;
    it->current = (Vertex*) src;
    it->stack_idx = -1;
    it->triangle_count = 0;
    it->visibility = 0;
    return (n) ? it : NULL;
}

ListIterator* _glIteratorNext(ListIterator* it);

#ifdef __cplusplus
}
#endif
