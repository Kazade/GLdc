#ifndef STACK_H
#define STACK_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned char* data;
    unsigned int capacity;
    unsigned int size;
    unsigned int element_size;
} Stack;

void init_stack(Stack* stack, unsigned int element_size, unsigned int capacity);
void* stack_top(Stack* stack);
void* stack_replace(Stack* stack, const void* element);
void* stack_push(Stack* stack, const void* element);
void stack_pop(Stack* stack);

#ifdef __cplusplus
}
#endif

#endif // STACK_H
