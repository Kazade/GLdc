#include <string.h>

#ifndef __APPLE__
#if defined(__WIN32__)
/* Linux + Kos define this, OSX does not, so just use malloc there */
#define memalign(x, size) malloc((size))
#else
#include <malloc.h>
#endif 
/* Linux + Kos define this, OSX does not, so just use malloc there */
#define memalign(x, size) malloc((size))
#endif

#include "stack.h"

void init_stack(Stack* stack, unsigned int element_size, unsigned int capacity) {
    stack->size = 0;
    stack->capacity = capacity;
    stack->element_size = element_size;
    stack->data = (unsigned char*) memalign(0x20, element_size * capacity);
}

void* stack_top(Stack* stack) {
    return &stack->data[(stack->size - 1) * stack->element_size];
}

void* stack_replace(Stack* stack, const void* element) {
    memcpy(stack->data + ((stack->size - 1) * stack->element_size), element, stack->element_size);
    return stack_top(stack);
}

void* stack_push(Stack* stack, const void* element) {
    if(stack->size + 1 == stack->capacity) {
        return NULL;
    }

    memcpy(stack->data + (stack->size * stack->element_size), element, stack->element_size);
    stack->size++;

    return stack_top(stack);
}

void stack_pop(Stack* stack) {
    if(stack->size == 0) {
        return;
    }

    stack->size--;
}
