#ifndef COMMON_H 
#define COMMON_H

#define LEFT_SHIFT(a, b)     ((a) << (b))
#define HIGHT_POW2(type)     ((type)1 << (sizeof(type)*8 - 1)) 

typedef enum { SUCCESS, FAILURE } result_t;

typedef struct string {
    char * s;
    size_t size;
} string_t;

typedef struct location {
    string_t urlw;
    string_t hostname;
    string_t port;
    unsigned short nport;
} location_t;

#endif /* COMMON_H */
