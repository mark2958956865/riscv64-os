#include <timeros/stack.h>

// 初始化栈
void initStack(Stack *stack) {
    stack->top = -1;
}

// 判断栈是否为空
bool isEmpty(Stack *stack) {
    return stack->top == -1;
}

// 判断栈是否已满
bool isFull(Stack *stack) {
    return stack->top == MAX_SIZE - 1;
}

// 入栈操作
void push(Stack *stack, uint64_t value) {
    if (isFull(stack)) {
        printk("Stack overflow\n");
        return;
    }
    stack->data[++stack->top] = value;
}

// 出栈操作
uint64_t pop(Stack *stack) {
    if (isEmpty(stack)) {
        printk("Stack underflow\n");
        return -1; // 表示栈为空或操作失败
    }
    return stack->data[stack->top--];
}

// 获取栈顶元素
uint64_t top(Stack *stack) {
    if (isEmpty(stack)) {
        printk("Stack is empty\n");
        return -1; // 表示栈为空
    }
    return stack->data[stack->top];
}
