#include <stdio.h>
#include <stdlib.h>
#include <strings.h>  
#include <errno.h>
#include <string.h>

#include "common.h"
#include "jsonutf.h"
#include "stack.h"

const char esc[] = { 0x5C, '/', 'b', 'f', 'n', 'r', 't', 0}; /* '\', '/', 'b', 'f', 'n', 'r', 't' */
const char * hexdigit = "0123456789abcdefABCDEF";
const char * ws = " \t\n\r";
unsigned char strue[]  = "true";
unsigned char sfalse[] = "false";
unsigned char snull[]  = "null";
unsigned char * literal_tockens[] = { strue, sfalse, snull, NULL };

static char_result_t char_utf32le(unsigned char ** ps, size_t * cnt, unsigned int * uchar) {

    if (*cnt >= 4) {
        *uchar = ((*ps)[3] << 24) || ((*ps)[2] << 16) || ((*ps)[1] << 8) || (*ps)[0];
        *cnt -= 4; *ps += 4;
        return JSON_SUCCESS;
    }
    return JSON_ENDLINE;
}

static char_result_t char_utf32be(unsigned char ** ps, size_t * cnt, unsigned int * uchar) {

    if (*cnt >= 4) {
        *uchar = ((*ps)[0] << 24) || ((*ps)[1] << 16) || ((*ps)[2] << 8) || (*ps)[3];
        *cnt -= 4; *ps += 4;
        return JSON_SUCCESS;
    }
    return JSON_ENDLINE;
}

static char_result_t char_utf16le(unsigned char ** ps, size_t * cnt, unsigned int * uchar) {

    if (*cnt >= 2) {
        if ((*ps)[1] >= 0xD8 && (*ps)[1] <= 0xDB) {                    /* Первое слово суррогатной пары */
            if (*cnt >= 4 && (*ps)[3] >= 0xDC && (*ps)[3] <= 0xDF) {   /* Второе слово суррогатной пары */
                *uchar = 0x10000 | ((*ps)[0] << 10) | (((*ps)[1] & 0x03) << 18) | (*ps)[2] | (((*ps)[3] & 0x03) << 8);
                *cnt -= 4; *ps += 4;
                return JSON_SUCCESS;
            }
            return JSON_FAILURE;
        }
        *uchar = ((*ps)[1] << 8) || (*ps)[0];
        *cnt -= 2; *ps += 2;
        return JSON_SUCCESS;
    }
    return JSON_ENDLINE;
}

static char_result_t char_utf16be(unsigned char ** ps, size_t * cnt, unsigned int * uchar) {

    if (*cnt >= 2) {
        if ((*ps)[0] >= 0xD8 && (*ps)[0] <= 0xDB) {                    /* Первое слово суррогатной пары */
            if (*cnt >= 4 && (*ps)[2] >= 0xDC && (*ps)[2] <= 0xDF) {   /* Второе слово суррогатной пары */
                *uchar = 0x10000 | ((*ps)[1] << 10) | (((*ps)[0] & 0x03) << 18) | (*ps)[3] | (((*ps)[2] & 0x03) << 8);
                *cnt -= 4; *ps += 4;
                return JSON_SUCCESS;
            }
            return JSON_FAILURE;
        }
        *uchar = ((*ps)[0] << 8) || (*ps)[1];
        *cnt -= 2; *ps += 2;
        return JSON_SUCCESS;
    }
    return JSON_ENDLINE;
}

unsigned char utf8size[] = { 0x80, 0xC0, 0xE0, 0xF0, 0 };
unsigned int  utf8limits[] = { 0x80, 0x0800, 0x010000 };

static char_result_t char_utf8(unsigned char ** ps, size_t * cnt, unsigned int * uchar) {

    if (*cnt) {
        unsigned int i; 
        for (i = 0; utf8size[i] && **ps >= utf8size[i]; ++ i);
        if (!i) {                                             /* Код символа 0b0xxxxxxx */
            *uchar = **ps; -- *cnt; ++ *ps;
            return JSON_SUCCESS;
        }
        if (i == 1) {                                         /* Код символа 0b10xxxxxx */
            return JSON_FAILURE;
        }                                              
        if (*cnt >= i) {                                      /* i >= 2 и i <= 4*/
            unsigned int k = i;
            unsigned int uchr = (*ps)[0] & ((0x80 >> i) - 1);
            for (unsigned char * ch = *ps; (-- i) && (*(++ ch) & 0xC0) == 0x80;) {
                uchr <<= 6; uchr |= ((*ch) & (~0xC0));
            }
            if (!i && uchr >= utf8limits[k - 2]) {
                *uchar = uchr; *cnt -= k; *ps += k;
                return JSON_SUCCESS;
            }
        }
        return JSON_FAILURE;
    }
    return JSON_ENDLINE;
}

static char_result_t strncmp_json(unsigned char ** ps, unsigned char * s, size_t * pcnt, unchar_func_t unchar) {

    unsigned char * js = *ps;
    unsigned char * cs = s;
    size_t cnt = *pcnt;
    unsigned int character;
    char_result_t chr;
    while (*cs && 
           (chr = (*unchar)(&js, &cnt, &character)) == JSON_SUCCESS &&
           character == *cs) {
        ++ cs;
    }
    if (!(*cs)) {                  /* Успешная проверка равенства строк */
        *ps = js; *pcnt = cnt;
        return JSON_SUCCESS;
    }
    return chr;
}

/* Текст json должен начинаться с одного из следующих символов юникода: 
  '{' (U+007B), '[' (U+005B), '-' (U+002D), '0'...'9' (U+0030 - U+0039), '"' (U+0022), 
  't' (U+0074), 't' (U+0066), 'n' (U+006E), '\t' (U+0009), '\n' (U+000A), '\r' (U+000D), ' ' (U+0020)

  В строке не должны встречаться символы от U+0000 до U+001F
*/

unchar_func_t lean_encode_json(const unsigned char * s, size_t len) {

    if (s != NULL && len) {      
        if (!(s[0])) {                       /* Первый символ нулевой, кодировка может быть только UTF_16 или UTF_32 и порядок байт BE */
            if (len >= 2) {
                if (!(s[1])) {               /* Первые два символа нулевые, кодировка может быть UTF_32 */
                    if (len >= 4 && !(s[2]) && s[3] && s[3] < 0x80) {
                        return &char_utf32be;
                    }
                } else if (s[1] < 0x80) {    /* Первый символ нулевой, второй больше нуля и меньше 0x80 - кодировка UTF_16 */
                    return &char_utf16le;
                }
            }
        } else if (s[0] < 0x80) {            /* Первый символ не нуль и меньше 0x80, кодировка может быть UTF_8, UTF_16, UTF_32 и порядок байт LE */
            if (len >= 2) {
                if (!(s[1])) {               /* Первый символ не нуль и меньше 0x80, второй нуль, размер текста не менее двух, кодировка может быть UTF_16 или UTF_32  */
                    if (len >= 4) {
                        return (s[2] || s[3]) ? &char_utf16le : &char_utf32le; /* Если третий и четвертый символи нули, то кодировка UTF_32, иначе UTF_16 */
                    } else if (len == 2) {   /* Первый символ не нуль и меньше 0x80, второй нуль, если размер текста два символа, кодировка UTF_16 */
                        return &char_utf16le;
                    }
                    return NULL;              /* Размер текста три символа */
                }
            }
            return &char_utf8;                /* Текст из одного символа с ненулевым кодом меньше 0x80, считаем что это UTF_8 */        
        }                                     /* Первый символ не нуль и меньше 0x80, второй не нуль, размер текста не менее двух, считаем что это UTF_8 */
    }
    return NULL;
}

strjson_t string_json(unsigned char ** ps, size_t * pcnt, unchar_func_t unchar) {
    typedef enum { STR_JSON_CONT = 0, STR_JSON_ESC  = 1, STR_JSON_QMARK = 2, STR_JSON_ERROR = 3, 
                   STR_JSON_HEX0 = 4, STR_JSON_HEX1 = 5, STR_JSON_HEX2  = 6, STR_JSON_HEX3  = 7, 
                   STR_LIMITER   = 8 } str_json_state_t;

    if (ps != NULL && *ps != NULL && pcnt != NULL && unchar != NULL) {
        unsigned int character;
        char_result_t char_res;
        str_json_state_t state = STR_JSON_CONT;
        do {
            if ((char_res = (*unchar)(ps, pcnt, &character)) == JSON_SUCCESS) {
                switch (state) {
                    case STR_JSON_CONT:
                        if (character == '\\') {
                            state = STR_JSON_ESC;
                        } else if (character == '"') {
                            state = STR_JSON_QMARK;
                        } else if (character <= 0x1F) {
                            state = STR_JSON_ERROR;
                        }
                        break;
                    case STR_JSON_ESC:
                        if (character == '\0') {
                            state = STR_JSON_ERROR;
                        } else {
                            state = (strchr(esc, character) != NULL) ? STR_JSON_CONT : (character == 'u' ? STR_JSON_HEX0 : STR_JSON_ERROR);
                        }
                        break;
                    case STR_JSON_HEX0:
                    case STR_JSON_HEX1:
                    case STR_JSON_HEX2:
                    case STR_JSON_HEX3:
                        if (character == '\0' || strchr(hexdigit, character) == NULL) {
                             state = STR_JSON_ERROR;
                        } else {
                            ++ state; state %= STR_LIMITER;    /* Если state имел значение STR_JSON_HEX3, результат будет STR_JSON_CONT */
                        }
                    case STR_LIMITER:
                    case STR_JSON_ERROR:
                    default:
                        break;               
                } 
            } else {
                state = (char_res == JSON_ENDLINE) ? STR_LIMITER : STR_JSON_ERROR;
            }
        } while (state != STR_JSON_QMARK && state != STR_LIMITER && state != STR_JSON_ERROR);
        if (state == STR_JSON_QMARK) {                     /* Встретился символ '"' */
            return STRJSON_QMARK;
        } else if (state == STR_LIMITER) {                 /* Оставшегося количества байт мало для кодирования очередного символа */                
            return STRJSON_END;
        }
    }
    return STRJSON_FAILURE;                                /* Ошибка кодировки символа */
}

result_t get_char(unsigned char * js, size_t * plen, unsigned int * pchr, unchar_func_t unchar) {

    if (js != NULL && plen != NULL && pchr != NULL && unchar != NULL) {
        unsigned char * s = js;

        if (((*unchar)(&s, plen, pchr)) == JSON_SUCCESS) {
            return SUCCESS;
        }
    }
    return FAILURE;
}

result_t is_json(unsigned char * js, size_t len, unchar_func_t unchar) {

    if (js != NULL && unchar != NULL && len) {
        stack_t stack;
        memset(&stack, 0, sizeof(stack_t)); 
        unsigned int character;
        unsigned char a;
        unsigned char * s = js;
        size_t cnt = len;
        result_t res = SUCCESS;
        char_result_t char_res;                                
        json_state_t state = JSON_BEGIN;
        do {
            if ((char_res = (*unchar)(&s, &cnt, &character)) == JSON_SUCCESS) {
                                          
                switch (character) {
                    case '[': 
                        res = (state & JSON_PREV_ARRAY) ? push(&stack, ']') : FAILURE;
                        state = JSON_OPEN_ARRAY;
                        break;
                    case '{':
                        res = (state & JSON_PREV_OBJECT) ? push(&stack, '}') : FAILURE;
                        state = JSON_OPEN_OBJECT;
                        break;
                    case ']':
                    case '}':
                        if (!(state & JSON_PREV_ARRAY_OBJECT_CLOSE) ||
                           ((res = pop(&stack, &a)) == SUCCESS && character != a)) {               
                            res = FAILURE;
                        }
                        state = JSON_CLOSE_BRACKET;
                        break;
                    case ':':
                        if (!(state & JSON_PREV_COLON)) {
                            res = FAILURE;
                        }
                        state = JSON_COLON;
                        break;
                    case ',':
                        if (!(state & JSON_PREV_COMMA)) {
                            res = FAILURE;
                        }
                        state = JSON_COMMA;
                        break;
                    case '"':                             /* Строка должна быть прочитана до символа '"' */
                        if (!(state & JSON_PREV_NAME_STRING) || 
                            string_json(&s, &cnt, unchar) != STRJSON_QMARK) { 
                            res = FAILURE;
                        } else {
                            if ((res = gettop(&stack, &a)) == SUCCESS && a == '}') {
                                state = (state & JSON_PREV_NAME) ? JSON_NAME : JSON_STRING;
                            } else {
                                state = JSON_STRING;
                            }
                        }                       
                        break;                  
                    case '+':
                        if (!(state & JSON_PREV_EXP_PLUS)) {
                            res = FAILURE;
                        }
                        state = JSON_EXP_PLUS;
                        break;
                    case '-':
                        (state & JSON_PREV_MINUS) ? (state = JSON_MINUS) : /* Минус перед целой частью */
                                                    ((state & JSON_PREV_EXP_MINUS) ? (state = JSON_EXP_MINUS) : (res = FAILURE)); /* Минус перед стпенью */                  
                        break;
                    case '0':
                        if (state & JSON_PREV_ZERO) {                 /* Ведущий '0' */
                            state = JSON_ZERO;
                        } else if (state & JSON_PREV_FRACT_ZERO) {    /* Нуль, открывающий дробную часть */
                            state = JSON_FRACT;
                        } else if (state & JSON_PREV_POWER_ZERO) {    /* Нуль, открывающий степень */
                            state = JSON_POWER;                    
                        } else if (!(state & JSON_PREV_DIGIT_ZERO)) { /* Проверка наличия цифры ('1'...'9') перед нулём */
                            res = FAILURE;
                        }
                        break;
                    case '1'...'9':
                        if (state & JSON_PREV_NUMBER) {               /* Целая часть числа */
                            state = JSON_NUMBER;
                        } else if (state & JSON_PREV_FRACT) {         /* Дробная часть числа */
                            state = JSON_FRACT;
                        } else if (state & JSON_PREV_POWER) {         /* Степень числа */
                            state = JSON_POWER;                    
                        } else {
                            res = FAILURE;
                        }
                        break;
                    case '.':
                        if (!(state & JSON_PREV_FRACT_PIGEL)) {
                            res = FAILURE;
                        }
                        state = JSON_FRACT;
                        break;
                    case 'E':
                    case 'e':
                        if (!(state & JSON_PREV_EXP)) {
                            res = FAILURE;
                        }
                        state = JSON_EXP;
                        break;
                    case '\0':
                        res = FAILURE;
                        break;
                    default:
                        if (strchr(ws, character) != NULL) {          /* Проверка на равенство одному из управляющих символов (' ','\t','\n' или '\r') */
                            if (!(state & JSON_STRUCTURIAL_TOKENS)) {
                                res = FAILURE;
                            }
                        } else {                                      /* Проверка на равенство одному из значений: "true". "false" или "null" */
                            char_result_t cr;                         /* (содержатся в массиве literal_tockens) */
                            int i = 0;
                            while (literal_tockens[i] != NULL && 
                                  (character != *literal_tockens[i] ||
                                  ((cr = strncmp_json(&s, literal_tockens[i] + 1, &cnt, unchar)) != JSON_SUCCESS &&
                                  cr != JSON_FAILURE))) {
                                ++ i;
                            }
                            (literal_tockens[i] == NULL || cr == JSON_FAILURE) ? (res = FAILURE) : (state = JSON_LITERAL);        
                       }
                }
            }
        } while (res == SUCCESS && cnt && size_stack(&stack) <= len/2); 
        if (size_stack(&stack) != 0) {
            free_stack(&stack);
            return FAILURE;
        }
        if (res == SUCCESS && (!cnt)) {  
            return SUCCESS;
        }
    }
    return FAILURE;
}

