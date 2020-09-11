#include <sys/types.h>
#include <stdlib.h>

#include "common.h"
#include "strcmpr.h"

static char toupper(char ch) {

    return (ch >= 'a' && ch <= 'z') ? (char)('A' + ch - 'a') : ch;
}

result_t strcmpr(const char *a, const char *b, size_t len) {

    if (a != NULL && b != NULL) {
        const char * sa = a;
        const char * sb = b;
        size_t n = len;
        for (;*sa && n && toupper(*sa) == toupper(*sb); -- n, ++ sa, ++ sb);
        if (!n) {
            return SUCCESS;
        }
    }
    return FAILURE;    
}
