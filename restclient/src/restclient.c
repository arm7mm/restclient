#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>    /* getaddrinfo */
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>  /* bcopy */
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>   

#include "common.h"
#include "jsonutf.h"
#include "url.h"
#include "strcmpr.h"
#include "restclient.h"

char post0[] = "POST ";
char post1[] = " HTTP/1.1\r\nHost: ";
char post2[] = ":";
char post3[] = "\r\nUser-Agent: restclient\r\nAccept: */*\r\nContent-Length: ";
char post4[] = "\r\nContent-Type: application/x-www-form-urlencoded\r\n\r\n{\"id\":0,\"jsonrpc\":\"2.0\",\"method\":\"";
char post5[] = "\",\"params\":";
char post6[] = "}";

char data[]  = "{\"id\":0,\"jsonrpc\":\"2.0\",\"method\":\"";

char limiter[2] = "\r\n";

#define POST_MIN_DATA_SIZE (sizeof(data) + sizeof(post5) + sizeof(post6) - 3)
#define POST_MIN_SIZE      (sizeof(post0) + sizeof(post1) + sizeof(post2) + sizeof(post3) + sizeof(post4) + sizeof(post5) + sizeof(post6) - 7)

#define SIZE_8CHARS(type)  (sizeof(type)*8)  
#define SIZE_CHARS         (SIZE_8CHARS(size_t))

char httphead[] = "HTTP/%1d.%1d %3d";

#define MIN_HTTP_SIZE      12

#define HTTP_CODE_OK       200
#define HTTP_CODE_LOCATION 300
#define HTTP_CODE_ERROR    400

typedef struct buffer {
    char * buf;          /* Адрес буфера */
    ssize_t readpos;     /* Позиция для чтения очередного фрагмента */
    ssize_t nread;       /* Количество значимых байт в буфере */
    size_t allc;         /* Размер буфера */
} buffer_t;

char clpref[] = "Content-Length:";
char locpref[] = "Location:";

string_t contlenpref = { clpref, sizeof(clpref) - 1 };
string_t locationpref = { locpref, sizeof(locpref) - 1 };

typedef enum { RECV_SUCCESS, RECV_REDIRECTION, RECV_ERROR, RECV_FAILURE } result_recv_t;

typedef struct msgset {
    string_t urlw;
    string_t hostname;
    string_t port;
    string_t method;
    string_t params;
} msgset_t;

msgset_t defmsg = { { DEFAULT_URL,  sizeof(DEFAULT_URL) - 1 },
                    { DEFAULT_HOST, sizeof(DEFAULT_HOST) - 1},
                    { DEFAULT_PORT, sizeof(DEFAULT_PORT) - 1},
                    { NULL, 0 },
                    { NULL, 0 } };

static int connect_server(struct sockaddr * paddr) {
    int sfd;

    printf("\nTry connect...\n");
    if ((sfd = socket(paddr->sa_family, SOCK_STREAM, 0)) == -1) {
        perror("socket");
    } else if (connect(sfd, (const struct sockaddr *)paddr, sizeof(struct sockaddr_in)) != -1) {
        return sfd;
    } else {
        close(sfd);
        perror("connect");
    }
    return -1;
}


static result_t search_server(const char * hostname, unsigned short port, int * psfd) {
    struct hostent * host;

    if ((host = gethostbyname(hostname)) != NULL) {
        char ** addrlist;
        if ((addrlist = host->h_addr_list) != NULL && *addrlist != NULL/* && host->h_length <= sizeof(addr.sin_addr.s_addr)*/) {
            int sfd;
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(struct sockaddr_in));            /* Очистка структуры sockaddr_in */
            addr.sin_family = host->h_addrtype;                      /* AF_INET или AF_INET6 */
            addr.sin_port = htons(port);                             /* порт */
            do {                                                     /* Попытка использовать каждый адрес до успешного подключения */
                bcopy(*addrlist, (char *)&addr.sin_addr.s_addr, host->h_length); /* IP адрес */
            } while ((sfd = connect_server((struct sockaddr *)&addr)) == -1 && *(++ addrlist) != NULL);
            if (*addrlist != NULL) {                                 /* Успешное подключение */
                *psfd = sfd;
                printf("Connect success to host %s:%d.\n", hostname, port);
                return SUCCESS;
            } else { 
                fprintf(stderr, "Could not connect (host %s:%d).\n", hostname, port);
            }
        } else {
            fprintf(stderr, "Not network addresses (host %s:%d).\n", hostname, port);
        }      
    } else {
        fprintf(stderr, "gethostbyname error (host %s:%d).\n", hostname, port);
    }
    return FAILURE;
}

static result_t msgsend(int psfd, msgset_t * msgs) {
    char * msg, * ps;
    size_t size;
    int  contlen;
    char cont[SIZE_CHARS];

    contlen = snprintf(cont, SIZE_CHARS, "%zu", POST_MIN_DATA_SIZE + msgs->method.size + msgs->params.size);
    if (contlen > 0 && contlen < SIZE_CHARS) {
        size = POST_MIN_SIZE + msgs->urlw.size + msgs->hostname.size +                          /* Размер */
               msgs->port.size + contlen + msgs->method.size + msgs->params.size;
        if ((msg = malloc(size)) != NULL) {                                                     /* Выделение памяти под сообщение */
            char * ps = msg;                  

            bcopy(post0, ps, sizeof(post0) - 1);              ps += sizeof(post0) - 1;
            bcopy(msgs->urlw.s, ps, msgs->urlw.size);         ps += msgs->urlw.size;
            bcopy(post1, ps, sizeof(post1) - 1);              ps += sizeof(post1) - 1;
            bcopy(msgs->hostname.s, ps, msgs->hostname.size); ps += msgs->hostname.size;
            bcopy(post2, ps, sizeof(post2) - 1);              ps += sizeof(post2) - 1;
            bcopy(msgs->port.s, ps, msgs->port.size);         ps += msgs->port.size;
            bcopy(post3, ps, sizeof(post3) - 1);              ps += sizeof(post3) - 1;
            bcopy(cont, ps, contlen);                         ps += contlen;
            bcopy(post4, ps, sizeof(post4) - 1);              ps += sizeof(post4) - 1;
            bcopy(msgs->method.s, ps, msgs->method.size);     ps += msgs->method.size;
            bcopy(post5, ps, sizeof(post5) - 1);              ps += sizeof(post5) - 1;
            bcopy(msgs->params.s, ps, msgs->params.size);     ps += msgs->params.size;
            bcopy(post6, ps, sizeof(post6) - 1);              ps += (sizeof(post6) - 1);            
            if (write(psfd, msg, size) != -1) {
                printf("Send POST message success.\n");
                free(msg);
                return SUCCESS;    
            } 
            perror("write");
            free(msg); 
        } else {
            perror("malloc");
        }
    }
    fprintf(stderr, "Send POST message error.\n");
    return FAILURE;
}


static result_t memresize(buffer_t * precv) {

    if (!(precv->allc & HIGHT_POW2(size_t))) {       /* Проверка размера памяти на переполнение */
        size_t nsize = precv->allc << 1;
        char * new = realloc(precv->buf, nsize);     /* Увеличение выделенной памяти в два раза */
        if (new != NULL) {
            precv->allc = nsize;
            precv->buf  = new;
            return SUCCESS; 
        } else {
            perror("realloc"); 
        }
    } else {
        fprintf(stderr, "Out_of_memory\n");
    }
    return FAILURE;
}

static result_t memsetresize(buffer_t * precv, size_t nsize) {

    char * new = realloc(precv->buf, nsize);     /* Увеличение выделенной памяти до значенмя nsize */
    if (new != NULL) {
        precv->allc = nsize;
        precv->buf  = new;
        return SUCCESS; 
    } else {
        perror("realloc"); 
    }
    return FAILURE;
}

/* Поиск ближайших справа последовательных символа s[2] начиная с позиции prv->buf + prv->readpos */
static char * find_chars(int psfd, buffer_t * prv, char s[2]) {
    char * endline = NULL;
    char * startline;
    ssize_t nread;
    size_t szread;
    result_t res = SUCCESS;

    do {       
        if (prv->allc == prv->nread) {                                /* После последнего чтения не осталось свободного места в буфере */
            res = memresize(prv);                                     /* Увеличение памяти для чтения в два раза */
        } 
        if (res == SUCCESS) {
            startline = prv->buf + prv->readpos;                      /* Новое значение startline */
            szread = prv->allc - prv->nread;
            if ((nread = read(psfd, prv->buf + prv->nread, szread)) > 0) { /* Чтение в буфер */
                prv->nread += nread;
            } else {
                nread == -1 ? perror("read") : fprintf(stderr, "Response in unsupported type.\n");
                res = FAILURE;
            }
            if (res == SUCCESS) {
                if ((endline = memchr(startline, s[1], nread)) != NULL) {   /* Символ chr[1] найден по адресу endline */
                    if (endline < prv->buf + prv->nread - 1) {              /* Между endline и prv->buf + prv->nread остались байты, */
                        prv->readpos = endline - prv->buf + 1;              /* среди которых тоже могут находииться символы s[2] */
                    }
                    if (*(endline - 1) != s[0]) {                           /* Проверка предыдущего символа */
                        endline = NULL;
                    }
                } else {
                    prv->readpos = prv->nread;
                }
            }
        }
    } while (res == SUCCESS && nread == szread && endline == NULL);
    return endline;
}

static result_t find_prefix(int psfd, buffer_t * prv, string_t * prefix, string_t * line) {
    char * start;
    char * next;

    if ((start = find_chars(psfd, prv, limiter)) != NULL) {              /* Найден разделитель '\r\n' */ 
        while ((next = find_chars(psfd, prv, limiter)) != NULL &&
              next - start - 1 > prefix->size &&
              strcmpr(prefix->s, start + 1, prefix->size) != SUCCESS) {
            start = next; 
        }
        if (next != NULL) {                                              /* Найдена строка с префиксом prefix */
            printf("Found head line \"%s\"\n", prefix->s);
            line->s = start + 1;
            line->size = next - start - 2;
            return SUCCESS; 
        }
    }
    fprintf(stderr, "Head line \"%s\" not found\n", prefix->s);
    return FAILURE; 
}

static result_t get_location(int psfd, buffer_t * prv, location_t * lct) {
    string_t location;

    if (find_prefix(psfd, prv, &locationpref, &location) == SUCCESS) {
        size_t pos;
        char * url = location.s + locationpref.size;
        size_t size = location.size - locationpref.size;

        return geturl(url, size, lct);
    }           
    return FAILURE; 
}

static result_t get_content(int psfd, buffer_t * prv, string_t * pcontent) {
    char * start;
    char * next;
    string_t contlen;

    if (find_prefix(psfd, prv, &contlenpref, &contlen) == SUCCESS) {
        char * pos = contlen.s + contlenpref.size;

        unsigned long long cnt = strtoull(pos, NULL, 10);
        if (cnt && cnt < ULLONG_MAX) {
            printf("Content Length is %llu\n", cnt);
            start = contlen.s + contlen.size;        
            while ((next = find_chars(psfd, prv, limiter)) != NULL &&
                 next - start > 2) {
                 start = next; 
            }
            if (next != NULL) {
                pcontent->size = cnt;
                size_t n = prv->buf + prv->nread - next - 1; /* Количество прочитанных байт после позиции next */
                if (cnt <= n) {                              /* Контент уже прочитан */
                    pcontent->s = next + 1;
                    return SUCCESS;
                } else {
                    size_t pos = next - prv->buf;
                    n = cnt - n;                              /* Количество байт которые необходимо прочитать */
                    if (memsetresize(prv, prv->nread + n) == SUCCESS) { /* Увеличение буфера */
                        ssize_t nread;
                        if ((nread = read(psfd, prv->buf + prv->nread, n)) != -1) {
                             prv->nread += nread;
                             pcontent->s = prv->buf + pos;
                             return (nread == n) ? SUCCESS : FAILURE;
                        } else {
                             perror("read");
                        }
                    }
                }
            }
        } else {
            fprintf(stderr, "Content not available\n");
        }
    }           
    return FAILURE; 
}

static result_recv_t msgrecv(int psfd, buffer_t * precv) {
    ssize_t nread;
    result_recv_t result = RECV_FAILURE;

    if ((precv->buf = malloc(MIN_HTTP_SIZE)) != NULL) {                 /* Выделение памяти под http заголовок */
        precv->allc = MIN_HTTP_SIZE;
        if ((nread = read(psfd, precv->buf, precv->allc)) != -1) {      /* Чтение http заголовка */
            int code;
            precv->nread   = nread;
            precv->readpos = nread;
            if (nread == MIN_HTTP_SIZE && sscanf(precv->buf, httphead, &code, &code, &code) == 3) {
                printf("Response at code is %d.\n", code);
                result = (code < HTTP_CODE_LOCATION) ? RECV_SUCCESS : (code < HTTP_CODE_ERROR ? RECV_REDIRECTION : RECV_ERROR);
            } else {        
                fprintf(stderr, "Response in unsupported type.\n");
            }
        } else {
            perror("read");
        }
        if (result != RECV_SUCCESS && result != RECV_REDIRECTION) {
            free(precv->buf);
            memset(precv, 0, sizeof(buffer_t));
        }
    } else {
        perror("malloc");
    }
    return result;
}

static result_t loadparam(buffer_t * pbuf) {
    FILE * pfd;
    ssize_t nread;
    size_t szread;
    result_t res = FAILURE;

    if ((pfd = fopen(INPUT_FILE, "r")) != NULL) {
        if ((pbuf->buf = malloc(MIN_SIZE_BUFFER)) != NULL) {
            pbuf->allc = MIN_SIZE_BUFFER;
            pbuf->nread = 0; res = SUCCESS;
            do {
                szread = pbuf->allc - pbuf->nread;
                if ((nread = fread(pbuf->buf + pbuf->nread, sizeof(char), szread, pfd)) != -1) {     
                    pbuf->nread += nread;
                    if (nread == szread) {
                        res = memresize(pbuf);
                    }
                } else {
                    fprintf(stderr, "fread failed\n");
                    res = FAILURE;
                }
            } while (res == SUCCESS && nread == szread);
            pbuf->readpos = pbuf->nread;                /* Количество байт для чтения */
            if (res != SUCCESS || pbuf->nread == 0) {
                free(pbuf);
                res = FAILURE;
                printf("Input not read from file.\n");
            } else {
                printf("Input read from file.\n");
            }
        } else {
            perror("malloc");
        }
        fclose(pfd);
    } else {
        perror("fopen");
    }
    return res;
}

static void savecontent(string_t * pcont, result_t json) {
    FILE * pfd;

    if ((pfd = fopen(OUTPUT_FILE, "w")) != NULL) {
        if (json == SUCCESS) {
            if (fwrite(pcont->s, sizeof(char), pcont->size, pfd) == pcont->size) {
                printf("Content saved to output file.\n"); 
            } else {
                fprintf(stderr, "fwrite failed\n");
            }
        }
        fclose(pfd);
    } else {
        perror("fopen");
    }
}

static result_t action(msgset_t * ppost, buffer_t * precv) {
    int sock; 
    char * hostname     = DEFAULT_HOST;
    char * hstnm        = NULL;
    unsigned short port = DEFAULT_NUM_PORT;
    result_recv_t code;
    int rdcnt = DEFAULT_MAX_REDIRECTION;
    result_t res;
    location_t loctn;
    string_t content;
    unchar_func_t fenc;

    memset(precv, 0, sizeof(buffer_t));
    do {
        if ((res = search_server(hostname, port, &sock)) == SUCCESS) {
            if (hstnm != NULL) {
                free(hstnm);
                hstnm = NULL;
            }

            if ((res = msgsend(sock, ppost)) == SUCCESS) {
                if (precv->buf != NULL) {
                    free(precv->buf);
                }
                memset(precv, 0, sizeof(buffer_t));
                code = msgrecv(sock, precv);
                switch (code) {
                    case RECV_SUCCESS :
                        if ((res = get_content(sock, precv, &content)) == SUCCESS) {
                            printf("Content is read\n");
                            result_t jsn = FAILURE;
                            if ((fenc = lean_encode_json((const unsigned char *)content.s, content.size)) != NULL &&   
                                is_json((unsigned char *)content.s, content.size, fenc) == SUCCESS) {
                                printf("Content is json type\n");
                                jsn = SUCCESS;
                            }
                            savecontent(&content, jsn);
                        }
                        break;
                    case RECV_REDIRECTION :
                        if ((res = get_location(sock, precv, &loctn)) == SUCCESS) {
                            size_t size = loctn.hostname.size + 1;
                            if (size && (hstnm = malloc(size)) != NULL) {
                                bcopy(loctn.hostname.s, hstnm, size);     /* Для функции gethostbyname требуется С строка */
                                hstnm[size - 1] = '\0';
                                hostname = hstnm;
                                ppost->urlw.s     = loctn.urlw.s;     ppost->urlw.size     = loctn.urlw.size;
                                ppost->hostname.s = loctn.hostname.s; ppost->hostname.size = loctn.hostname.size;
                                ppost->port.s     = loctn.port.s;     ppost->port.size     = loctn.port.size;
                            
                                port = loctn.nport;
                            } else {
                                res = FAILURE;
                            }
                        }
                        break;
                    case RECV_ERROR :
                        break;
                    default:
                    case RECV_FAILURE :
                        break;
                }
            }
            close(sock);
        } 
    } while (res == SUCCESS && code == RECV_REDIRECTION && -- rdcnt);
    if (hstnm != NULL) {
        free(hstnm);
    }
    if (precv->buf != NULL) {
        free(precv->buf);
    }
    if (!rdcnt) {
        printf("Maximum number of redirects reached (%d) \n", DEFAULT_MAX_REDIRECTION);
    } 
    return SUCCESS; 
}

int main() {
    unchar_func_t fenc;
    buffer_t in;
    unsigned int chr;

    if (loadparam(&in) == SUCCESS) {                                                               /* Чтение входных данных из файла */
        unsigned char * ptr = (unsigned char *)in.buf;                                             /* Позиция начала чтения */
                                                                                                                            
        if ((fenc = lean_encode_json((const unsigned char *)ptr, in.readpos)) != NULL &&           /* Определение кодировки */
            get_char(ptr ++, (size_t *)&in.readpos, &chr, fenc) == SUCCESS &&                      /* Первый символ в файле должен быть '"' */
            chr == '"' &&
            string_json(&ptr, (size_t *)&in.readpos, fenc) == STRJSON_QMARK) {                     /* Чтение строки до следующего символ '"' */

            msgset_t post;                                                                         /* Структура POST запроса */
            printf("Name method is json type.\n");

            bcopy((char *)&defmsg, (char *)&post, sizeof(msgset_t));
            post.method.s    = in.buf + 1;
            post.method.size = in.nread - in.readpos - 2;

            if (is_json(ptr, in.readpos, fenc) == SUCCESS) {
                printf("Method parameters is json type.\n");

                buffer_t recv;            /* Структура буфера для ответа */

                post.params.s    = (char *)ptr;
                post.params.size = in.readpos;
 
                action(&post, &recv);
                free(in.buf);             /* Освобождение входного буфера */   
                exit(EXIT_SUCCESS);
            } else {
               fprintf(stderr, "Method parameters is not json type.\n");
            }
        } else {
            fprintf(stderr, "Name method is not json type.\n");
        }
        free(in.buf);
        exit(EXIT_FAILURE);
    }
}
