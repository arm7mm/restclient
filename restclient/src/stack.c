#include <stdio.h>
#include <stdlib.h>
#include <strings.h>  /* bcopy */
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "common.h"
#include "stack.h"

result_t push(stack_t * stk, unsigned char a) {

    if (stk != NULL) {
        if (stk->beg == NULL) {                                      /* В стеке нет элементов */
            if ((stk->beg = malloc(MIN_FIRST_ALLOC)) != NULL) {      /* Выделение памяти */
                stk->allc = MIN_FIRST_ALLOC;
                stk->size = 1;
                stk->beg[0] = a;
            } else {
                perror("malloc");
                return FAILURE;
            }
        } else {                                                     /* В стеке уже содержатся элементы */
            if (stk->size == stk->allc) {                            /* Количество элементов достигло размера доступной памяти */
                if (!(stk->allc & HIGHT_POW2(size_t))) {             /* Размер допустимой памяти не достиг порога переполнения */
                    size_t allc = stk->allc << 1;
                    unsigned char * new = realloc(stk->beg, allc); /* При успехе память под стек будет в два раза больше */
                    if (new != NULL) {
                        stk->allc = allc;
                        stk->beg  = new;
                    } else {
                        perror("realloc"); 
                        return FAILURE;
                    }
                } else {
                    fprintf(stderr, "Out_of_memory\n");
                    return FAILURE;
                }
            }
            stk->beg[stk->size ++] = a;                               /* Здесь stk->size < stk->allc */
        }
        return SUCCESS;
    }
    return FAILURE; 
}
        
result_t pop(stack_t * stk, unsigned char * a) {

    if (stk != NULL && stk->beg != NULL && a != NULL) {
        *a = stk->beg[-- stk->size];
        if (stk->size == 0) {
            free(stk->beg);
            stk->beg  = NULL;
            stk->allc = 0;
        }
        return SUCCESS;
    }
    return FAILURE;
}

result_t gettop(stack_t * stk, unsigned char * a) {

    if (stk != NULL && stk->beg != NULL && a != NULL) {
        *a = stk->beg[stk->size - 1];
        return SUCCESS;
    }
    return FAILURE;
}

size_t size_stack(stack_t * stk) {

    return (stk != NULL) ? stk->size : 0;
}

void free_stack(stack_t * stk) {

    if (stk != NULL && stk->beg != NULL) {
        free(stk->beg);
        memset(stk, 0, sizeof(stack_t));
    }
}
