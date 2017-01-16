/* 
 * Copyright (c) 2015 Kaerus Software AB, all rights reserved.
 * Author Anders Elo <anders @ kaerus com>.
 *
 * Licensed under Propreitary Software License terms, (the "License");
 * you may not use this file unless you have obtained a License.
 * You can obtain a License by contacting < contact @ kaerus com >. 
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef _POSTREST_DB_H_
#define _POSTREST_DB_H_

#ifdef __cplusplus
extern "C" {
#endif

#define ADAPTER_VERSION 100 // 00 01 00

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define Reply(args...) xbuf_xcat(rep,args)
#define ReplyError(code) Reply("<h1>%s</h1><p>%s</p>",http_status(code),http_error(code))
#define ReplyJsError(code,details) Reply("{\"code\":%d,\"error\":\"%s\",\"message\":\"%s\",\"details\":\"%s\"}",code, http_status(code), http_error(code), details ? details : "")

#define DB_READ   0
#define DB_WRITE  1

#define MAX_HEADERS 12  // maximum http request headers allowed

#ifdef DEBUG
#define DEBUG_PRINT 1
#else
#define DEBUG_PRINT 0
#endif

#define debug_printf(fmt, ...)						\
    do {								\
	tm_t t;								\
	s_gmtime(s_time(),&t);						\
	u32 us = (u32) getus() % 1000000;				\
	const char *file = strrchr(__FILE__,'#');			\
	if (DEBUG_PRINT) fprintf(stderr, "[%02d:%02d:%02d-%06d]%s:%d:%s(): " fmt, \
				 t.tm_hour,t.tm_min,t.tm_sec,us,	\
				 file ? file : __FILE__, __LINE__, __func__, ##__VA_ARGS__); } \
    while (0)


#define HTTP_600_DB_ERROR   600
    
    typedef struct {
	u64 timestamp;
	u32 timeout;
	u32 count;
	void *conn;
	char *skey;
	char *name;
	char *schema;
	char *user;
	char *pass;
	char *host;
	char *port;
	char *options;
    } db_t;

    typedef struct http_headers {
	char name[64];
	char value[192];
    } http_headers_t;

    typedef kv_t session_t;

    xbuf_t *get_request(char **);
    db_t *db_session(char **);
    db_t *db_connect(char **, char *, char *);
    int db_close(char **);
    void db_free(char **, db_t *);
    int db_response(db_t *, xbuf_t *, char **);
    int db_send(db_t *, xbuf_t *, char *);
    int db_error(xbuf_t *, int, char *);
    int db_wait(void *, int, struct timeval *);

    int bad_request(char **, int, char *);
    int get_headers(xbuf_t *, http_headers_t *, int);
    int get_header_value(http_headers_t *, char *, char **);

    int strdigit(char *);
    int strcsv(char *);
    int strword(char *);
    int isUUID(char *);

    char *unquote(char *);
    char *strjoin(xbuf_t *, char **, int, char, char, char);

#ifdef __cplusplus
}
#endif

#endif // _POSTREST_DB_H_
