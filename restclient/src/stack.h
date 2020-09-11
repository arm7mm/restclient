#ifndef STACK_H 
#define STACK_H

#define MIN_FIRST_ALLOC 32

typedef struct stack {
    unsigned char * beg;          /* Начало стека */
    size_t size;                  /* Размер стека */ 
    size_t allc;                  /* Резмер доступной памяти */
} stack_t;

extern result_t push(stack_t * stk, unsigned char a);
extern result_t pop(stack_t * stk, unsigned char * a);
extern result_t gettop(stack_t * stk, unsigned char * a);
extern size_t size_stack(stack_t * stk);
extern void free_stack(stack_t * stk);

#endif /* STACK_H */
