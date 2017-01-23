/* 
 * Copyright (c) 2015 Kaerus Software AB, all rights reserved.
 * Author Anders Elo <anders @ kaerus com>.
 *
 * Licensed under Apache 2.0 Software License terms, (the "License");
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _POSTREST_DB_H_
#define _POSTREST_DB_H_

#ifdef __cplusplus
extern "C" {
#endif

#define ADAPTER_VERSION 100 // 00 01 00

#define MAX(a,b) ((a) > (b) ? a : b)
#define MIN(a,b) ((a) < (b) ? a : b)
    
#define ArrayCount(arr) (sizeof(arr)/sizeof(*arr))

#define Reply(args...) xbuf_xcat(rep,args)

#define ReplyError(code)                                                            \
    Reply("<h1>%s</h1><p>%s</p>",                                                   \
    http_status(code),http_error(code))
    
#define ReplyJsError(code,details)                                                  \
    Reply("{\"code\":%d,\"error\":\"%s\",\"message\":\"%s\",\"details\":\"%s\"}",   \
    code, http_status(code), http_error(code), details ? details : "")

#define DB_READ   0
#define DB_WRITE  1

#define MAX_HEADERS 12  // maximum http request headers allowed

#ifdef DEBUG
#define DEBUG_PRINT 1
#else
#define DEBUG_PRINT 0
#endif

#define debug_printf(fmt, ...)						                            \
    if (DEBUG_PRINT) do {								                        \
	tm_t t;								                                        \
	s_gmtime(s_time(),&t);						                                \
	u32 us = (u32) getus() % 1000000;				                            \
	const char *file = strrchr(__FILE__,'#');			                        \
	fprintf(stderr, "[%02d:%02d:%02d-%06d]%s:%d:%s(): " fmt,                    \
				 t.tm_hour,t.tm_min,t.tm_sec,us,	                                \
				 file ? file : __FILE__, __LINE__, __func__, ##__VA_ARGS__); }  \
    while (0)


#define HTTP_600_DB_ERROR   600

#define DB_SESSION_TIMEOUT 300*1000;
    
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

    typedef struct {
        kv_t *sessions;
        kv_t *config;
    } server_t;

#define MAX_HEADER_NAME 64
#define MAX_HEADER_VALUE 256
    
    typedef struct http_headers {
	    char name[MAX_HEADER_NAME];
	    char value[MAX_HEADER_VALUE];
    } http_headers_t;

    typedef kv_t session_t;

    typedef struct {
        void *base;
        short val;
    } db_wait_t;

    xbuf_t *get_request(char **);
    db_t *db_session(char **);
    db_t *db_connect(char **, char *, char *);
    int db_close(char **);
    void db_free(char **, db_t *);
    int db_response(db_t *, xbuf_t *, char **);
    int db_send(db_t *, xbuf_t *, char *);
    int db_error(xbuf_t *, int, char *);
    int db_wait(void *, int, struct timeval *, short *);
    void db_timer_cb(int, short, void *);
        
    int bad_request(char **, int, char *);
    int get_headers(xbuf_t *, http_headers_t *, int);
    int get_header_value(http_headers_t *, char *, char **);

    int strdigit(char *);
    int strcsv(char *);
    int strword(char *);
    int isUUID(char *);
    int isString(char *);
    int isToken(char *);
    int isBase64(char *);
    char *unquote(char *);
    void url_decode(char *, const char *);
    char *strjoin(xbuf_t *, char **, int, char, char, char);
    void stringToLowercase(char *, int);

#define ENTRYPOINT_METHOD(name) name(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db)
#define GET_METHOD(name) int get_##ENTRYPOINT_METHOD(name)
#define PUT_METHOD(name) int put_##ENTRYPOINT_METHOD(name)
#define POST_METHOD(name) int post_##ENTRYPOINT_METHOD(name)
#define PATCH_METHOD(name) int patch_##ENTRYPOINT_METHOD(name)
#define DELETE_METHOD(name) int delete_##ENTRYPOINT_METHOD(name)

#define EP_PERMIT_UNAUTH 0x1  // permit unauthenticated access
    
    typedef int (EndpointFunction)(int, char **, xbuf_t *, xbuf_t *, db_t *);
    
    typedef struct {
        int argc;
        char **argv;
        xbuf_t *req;
        xbuf_t *rep;
        db_t *db;
    } EndpointArgs;

    
    typedef struct {
        int method;
        char *cmd;
        EndpointFunction *endpoint;
        int flags;
    } EndpointEntry;
    
    int exec_endpoint(int, char **, EndpointEntry [], int);
    
#ifdef __cplusplus
}
#endif

#endif // _POSTREST_DB_H_
