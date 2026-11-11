#ifndef TOP_STACK_H__
#define TOP_STACK_H__

#include "os.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_SIZE 10000

typedef struct {
    uint64_t data[MAX_SIZE];
    int top;
} Stack;

void initStack(Stack *stack);
bool isEmpty(Stack *stack);
bool isFull(Stack *stack);
void push(Stack *stack, uint64_t value);
uint64_t pop(Stack *stack);
uint64_t top(Stack *stack);

#endif