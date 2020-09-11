#ifndef JSON_UTF_H 
#define JSON_UTF_H

typedef enum { JSON_SUCCESS, JSON_ENDLINE, JSON_FAILURE } char_result_t;

typedef enum { STRJSON_QMARK, STRJSON_END, STRJSON_FAILURE } strjson_t;

/* Символы ' ', '\t', '\n', '\r' не меняют состояния */ 
typedef enum { JSON_BEGIN         = LEFT_SHIFT(1, 0),    /* */ 
               JSON_OPEN_ARRAY    = LEFT_SHIFT(1, 1),    /* '[' */
               JSON_OPEN_OBJECT   = LEFT_SHIFT(1, 2),    /* '{' */
               JSON_CLOSE_BRACKET = LEFT_SHIFT(1, 3),    /* ']' и '}' */
               JSON_COLON         = LEFT_SHIFT(1, 4),    /* ':' */ 
               JSON_COMMA         = LEFT_SHIFT(1, 5),    /* ',' */
               JSON_STRING        = LEFT_SHIFT(1, 6),    /* Строка значения */
               JSON_NAME          = LEFT_SHIFT(1, 7),    /* Строка имени */
               JSON_MINUS         = LEFT_SHIFT(1, 8),    /* Знак '-' (отрицательное число) */
               JSON_ZERO          = LEFT_SHIFT(1, 9),    /* Ведущий '0' */
               JSON_NUMBER        = LEFT_SHIFT(1, 10),   /* Целая часть числа цифры ('1'...'9') */
               JSON_FRACT_PIGEL   = LEFT_SHIFT(1, 11),   /* Точка, ожидание дробной части числа */
               JSON_FRACT         = LEFT_SHIFT(1, 12),   /* Дробнаю часть числа (цифры '0'...'9') */
               JSON_EXP           = LEFT_SHIFT(1, 13),   /* Экспонента 'e' или 'E' */
               JSON_EXP_PLUS      = LEFT_SHIFT(1, 14),   /* Знак '+' после 'e' или 'E' */
               JSON_EXP_MINUS     = LEFT_SHIFT(1, 15),   /* Знак '-' после 'e' или 'E' */
               JSON_POWER         = LEFT_SHIFT(1, 16),   /* Степень числа */
               JSON_LITERAL       = LEFT_SHIFT(1, 17),   /* "true", "false" или "null" */
             } json_state_t;

#define JSON_PREV_ARRAY              (JSON_BEGIN | JSON_OPEN_ARRAY | JSON_COLON | JSON_COMMA) 
#define JSON_PREV_OBJECT             (JSON_BEGIN | JSON_OPEN_ARRAY | JSON_COLON | JSON_COMMA)

#define JSON_PREV_ARRAY_CLOSE        (JSON_OPEN_ARRAY | JSON_CLOSE_BRACKET | JSON_STRING | JSON_ZERO | JSON_NUMBER | JSON_FRACT | JSON_POWER | JSON_LITERAL)
#define JSON_PREV_OBJECT_CLOSE       (JSON_OPEN_OBJECT | JSON_CLOSE_BRACKET | JSON_STRING | JSON_ZERO| JSON_NUMBER | JSON_FRACT | JSON_POWER | JSON_LITERAL)
#define JSON_PREV_ARRAY_OBJECT_CLOSE (JSON_PREV_ARRAY_CLOSE | JSON_PREV_OBJECT_CLOSE)

#define JSON_PREV_COLON              (JSON_NAME)
#define JSON_PREV_COMMA              (JSON_CLOSE_BRACKET | JSON_STRING | JSON_ZERO | JSON_NUMBER | JSON_FRACT | JSON_POWER | JSON_LITERAL)

#define JSON_PREV_MINUS              (JSON_BEGIN | JSON_OPEN_ARRAY | JSON_COLON | JSON_COMMA)

#define JSON_PREV_EXP_PLUS           (JSON_EXP)
#define JSON_PREV_EXP_MINUS          (JSON_EXP)

#define JSON_PREV_ZERO               (JSON_BEGIN | JSON_OPEN_ARRAY | JSON_COLON | JSON_COMMA | JSON_MINUS)
#define JSON_PREV_FRACT_ZERO         (JSON_FRACT_PIGEL)
#define JSON_PREV_POWER_ZERO         (JSON_EXP | JSON_EXP_PLUS | JSON_EXP_MINUS)
#define JSON_PREV_DIGIT_ZERO         (JSON_NUMBER | JSON_FRACT | JSON_POWER)

#define JSON_PREV_NUMBER             (JSON_BEGIN | JSON_OPEN_ARRAY | JSON_COLON | JSON_COMMA | JSON_MINUS | JSON_NUMBER)
#define JSON_PREV_FRACT              (JSON_FRACT_PIGEL | JSON_FRACT)
#define JSON_PREV_POWER              (JSON_EXP_PLUS | JSON_EXP_MINUS | JSON_POWER)

#define JSON_PREV_FRACT_PIGEL        (JSON_ZERO | JSON_NUMBER)

#define JSON_PREV_EXP                (JSON_ZERO | JSON_NUMBER | JSON_FRACT)

#define JSON_STRUCTURIAL_TOKENS      (JSON_BEGIN | JSON_OPEN_ARRAY | JSON_OPEN_OBJECT | JSON_CLOSE_BRACKET | JSON_COLON | JSON_COMMA | JSON_STRING | JSON_NAME | JSON_ZERO | JSON_NUMBER | JSON_FRACT | JSON_POWER | JSON_LITERAL)
#define JSON_PREV_NAME               (JSON_OPEN_OBJECT | JSON_COMMA)
#define JSON_PREV_NAME_STRING        (JSON_BEGIN | JSON_OPEN_ARRAY | JSON_COLON | JSON_PREV_NAME)
typedef char_result_t (* unchar_func_t)(unsigned char **, size_t *, unsigned int *);

extern unchar_func_t lean_encode_json(const unsigned char * s, size_t len);
extern result_t get_char(unsigned char * js, size_t * plen, unsigned int * pchr, unchar_func_t unchar);
extern strjson_t string_json(unsigned char ** ps, size_t * pcnt, unchar_func_t unchar);
extern result_t is_json(unsigned char * js, size_t len, unchar_func_t unchar);

#endif  /* JSON_UTF_H */
