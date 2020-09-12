#include <stdio.h>
#include <stdlib.h>
#include <strings.h>  
#include <string.h>
#include <limits.h>

#include "common.h"
#include "restclient.h"
#include "url.h"
#include "strcmpr.h"

result_t geturl(char * url, size_t size, location_t * loc) {
    typedef enum { URL_SCHEME = 0, URL_USERINFO = 1, URL_HOSTNAME = 2, 
                   URL_PORT = 3, URL_PATH = 4, URL_QUERY = 5, URL_FRAGMENT = 6, URL_WS = 7, URL_ERROR } urlstate_t;

    if (url != NULL && loc != NULL && size) {
        char * u = url;
        char * hostname;
        char * port = NULL;
        char * urlw = NULL;
        size_t sz = size;
        urlstate_t state = URL_USERINFO;
     
        memset(loc, 0, sizeof(location_t));                                 /* очистка структуры location */

        for (;*u == ' ' && sz; ++ u, -- sz);                                /* Пропуск ведущих пробелов */

        if (sz >= sizeof(HTTP_PREF) && 
           strcmpr(HTTP_PREF, u, sizeof(HTTP_PREF) - 1) == SUCCESS) {       /* Проверка префикса "http://" */
            u += (sizeof(HTTP_PREF) - 1);
            sz -= (sizeof(HTTP_PREF) - 1);
            hostname = u;                                                    /* Позиция возможного начала имени хоста */
            unsigned int nport = 0;
            size_t len = 0; 
            size_t portlen = 0; 
            while (sz && *u && state != URL_FRAGMENT && state != URL_ERROR &&
                  (state != URL_WS ||  *u == ' ')) {
                if (len == 1 && state == URL_HOSTNAME) {
                    hostname = u;
                }
                switch (*u) {
                    case '@':
                        if (state == URL_USERINFO) {
                            len = 0; state = URL_HOSTNAME;
                        } else if (state == URL_PORT) {
                            state = URL_ERROR;
                        }
                        break;
                    case ':':
                        if (state == URL_USERINFO || state == URL_HOSTNAME) {
                            loc->hostname.size = len;       /* Размер строки имени хоста */
                            state = URL_PORT;
                        } else if (state == URL_PORT) {
                            state = URL_ERROR;
                        } 
                        break;
                    case '/':
                        switch(state) {
                            case URL_USERINFO :
                            case URL_HOSTNAME :
                                loc->hostname.size = len;   /* Размер строки имени хоста */
                            case URL_PORT :
                                len = 0;
                                urlw = u; state = URL_PATH;
                                break;
                            case URL_PATH :
                                if (*(u - 1) == *u) {                            
                                    state = URL_ERROR;
                                }
                                loc->urlw.size = len;  
                            default:                           
                                break;
                        }
                        break;
                    case '0'...'9':
                        if (state == URL_PORT) {
                            nport *= 10; nport += (*u - 0x30);
                            if (!(portlen ++)) {
                                port = u;
                            }
                            if (nport > USHRT_MAX) {
                                state = URL_ERROR;
                            }
                        }
                        break;
                    case '#':
                        switch(state) {
                            case URL_USERINFO :
                            case URL_HOSTNAME :
                                loc->hostname.size = len;       /* Размер строки имени хоста */
                            case URL_PORT :
                                break;
                            case URL_PATH :
                                loc->urlw.size = len;
                            default:
                                break;
                        }
                        state = URL_FRAGMENT;
                    default:
                        if (state == URL_PORT) {
                            state = (*u == ' ') ? URL_WS : URL_ERROR;
                        } 
                        break;
                }
                len ++; -- sz; ++ u;
            }
            if (state != URL_ERROR && (!sz || state == URL_FRAGMENT)) {
                if (state != URL_FRAGMENT) {
                    for (sz = 0; *(-- u) == ' '; ++ sz);         /* Пробелы в конце строки */
                    if (*u == '/') {
                        ++ sz;
                    }
                    if (state == URL_USERINFO || state == URL_HOSTNAME) {
                        loc->hostname.size = len - sz;
                    } else if (state == URL_PATH) {
                        loc->urlw.size = len - sz;
                    }
                }
                loc->hostname.s    = hostname;
                if (port != NULL) {
                    loc->port.s    = port;                          /* Порт */
                    loc->port.size = portlen;                       /* Размер строки порта */
                    loc->nport     = nport;
                } else {
                    loc->port.s    = HTTP_PORT;
                    loc->port.size = sizeof(HTTP_PORT) - 1;
                    loc->nport     = HTTP_NUM_PORT;
                }
                loc->urlw.s = urlw;
                return SUCCESS;
            }                                                        /* Встретился нулевой символ */
        }       
    }
    fprintf(stderr, "Url error\n");
    return FAILURE;
}

