/* 
 * Copyright (c) 2015 Kaerus Software AB, all rights reserved.
 * Author Anders Elo <anders @ kaerus com>.
 *
 * Licensed under Apache 2.0 Software License terms, (the "License");
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <ctype.h>


/*
 * Determine whether a given character is a space character according to
 * http://tools.ietf.org/html/draft-ietf-httpbis-p1-messaging#section-3.2.6
 */
#define IS_SPACE(c) (c == ' ' || c == '\t')
#define IS_ALPHA(c) ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) 
#define IS_NUMBER(c) (c >= '0' && c <= '9')

/* 
 * Http header allowed characters as per RFC 2616 
 * Regexp: /^[a-zA-Z0-9_!#$%&'*+.^`|~-]+$/               
 */ 

/*
 * Determine whether a given character is a token character according to
 * http://tools.ietf.org/html/draft-ietf-httpbis-p1-messaging#section-3.2.6
 *      tchar          = "!" / "#" / "$" / "%" / "&" / "'" / "*"
 *                    /  "+" / "-" / "." / "^" / "_" / "`" / "|" / "~"
 *                    / DIGIT / ALPHA
 */
#define IS_TOKEN(c) ( \
        IS_ALPHA(c)   \
    || IS_NUMBER(c)   \
    || c == '!'       \
    || c == '#'       \
    || c == '$'       \
    || c == '%'       \
    || c == '&'       \
    || c == 0x27      \
    || c == '*'       \
    || c == '+'       \
    || c == '-'       \
    || c == '.'       \
    || c == '^'       \
    || c == '_'       \
    || c == '`'       \
    || c == '|'       \
    || c == '~'       \
    )

// ASCII hex values
#define ASCII_SQ    0x27 // single quote
#define ASCII_SPACE 0x20 // space

/* removes double quotes and control codes from string */
char *unquote(char *string){
    int length = strlen(string);
    
    for(int i = 0; i < length; i++){
        if(string[i] == '"') string[i] = ASCII_SQ;          // replaces double quote with single quote
        else if(string[i] < ASCII_SPACE) string[i] = '^';   // replaces control codes with ^.
    }
    
    return string;
}

char *strjoin(xbuf_t *buf, char **list, int len, char start, char stop, char delim){
    if(len < 1) return 0;
    
    if(start) xbuf_xcat(buf,"%c",start);
    
    int i;
    
    for(i = len - 1; i > 0; i--){
        xbuf_xcat(buf,"%s,",*list);
        list++;
    }
    
    if(!i) xbuf_xcat(buf,"%s",*list);
    
    if(stop) xbuf_xcat(buf,"%c",stop);
    
    return buf->ptr;
}

int strword(char *string){
    if(!string) return 0;
    
    int len = strlen(string);
    
    if(len <= 0) return 0;

    char c;
    
    for(int i = 0; i < len; i++){
        c = string[i];
        
        // allows: - _ 0-9 A-Z a-z
        // todo: consider using bitmasks
        if(!(c ==0x2d || c==0x5f) && 
           ((c < 0x30 || c > 0x7a) ||  
            (c > 0x39 && c < 0x41) ||   
            (c > 0x5a && c < 0x61))) {  
            return 0;
        }
    }
    
    return 1;
}

int isString(char *string){
    if(!string) return 0;

    int len = strlen(string);

    if(len <= 0) return 0;

    char c;

    for(int i = 0; i < len; i++){
	    c = string[i];

	    // reject: nonascii ' ;
	    if(c < 0x20 || c == 0x27 || c == 0x3b) return 0;
    }

    return 1;
}


int isToken(char *string){
    if(!string) return 0;

    int len = strlen(string);

    char c;

    for(int i = 0; i < len; i++){
        c = string[i];
        if(!IS_TOKEN(c) ) return 0;
    }

    return 1;
}

// validate uuid string
// aa196922-4471-4c1f-aa4a-f8d7bdb39bff
int isUUID(char *string){
    if(!string) return 0;

    int len = strlen(string);
    
    if(len != 36) return 0;

    char c;
    
    for(int i = 0; i < len; i++){
	    c = string[i];
	    
	    if(((c < '0' || c > 'f') ||
	        (c > '9' && c < 'a') || 
	        (c != '-'))) return 0;
    }

    return 1;
}

int isBase64(char *string){
    if(!string) return 0;

    int len = strlen(string);

    if(len <= 0) return 0;

    char c;

    for(int i = 0; i < len; i++){
	    c = string[i];

	    if(!( IS_ALPHA(c)   || 
	          IS_NUMBER(c)  || 
	          c == '+'      || 
	          c == '/'      || 
	          c == '=') ) return 0;
    }

    return 1;
}

/* check that string is a valid csv list */
int strcsv(char *string){
    if(!string) return 0;

    int len = strlen(string);

    if(len <= 0) return 0;

    char c;
    
    for(int i = 0; i < strlen(string); i++){
        c = string[i];
        
        if( c < 0x2c                || 
            c > 0x7a                || 
            (c > 0x3a && c < 0x40)  || 
            (c > 0x5a && c < 0x61)) return 0;
    }
    
    return 1;
}

int strdigit(char *string){
    if(!string) return 0;

    int len = strlen(string);

    if(len <= 0) return 0;

    char c;
    
    for(int i = 0; i < strlen(string); i++){
        c = string[i];
        
        if(c < 0x30 || c > 0x39) return 0;
    }

    return 1;
}

void url_decode(char *dst, const char *src)
{
    char a, b;
    while (*src) {
	    if ((*src == '%') &&
	        ((a = src[1]) && (b = src[2])) &&
	        (isxdigit(a) && isxdigit(b))) {
	        if (a >= 'a')
		    a -= 'a'-'A';
	        if (a >= 'A')
		    a -= ('A' - 10);
	        else
		    a -= '0';
	        if (b >= 'a')
		    b -= 'a'-'A';
	        if (b >= 'A')
		    b -= ('A' - 10);
	        else
		    b -= '0';
	        *dst++ = 16*a+b;
	        src+=3;
	    } else {
	        *dst++ = *src++;
	    }
    }
    *dst++ = '\0';
}

xbuf_t *get_request(char **argv) {
    return (xbuf_t *) get_env(argv,READ_XBUF);
}

void stringToLowercase(char *string, int len){
    if(!len) len = strlen(string);
    
    for(int i = 0; i < len; i++){
        string[i] = tolower(string[i]);
    }
}

/* parse and truncate http headers */
int get_headers(xbuf_t *req, http_headers_t *headers, int maxh){
    char *delim, *name ,*value;
    
    u32 len = req->len;
    char *p1, *p2;
    
    int count = 0, hlen, nlen, vlen;
    
    p1 = req->ptr;
   
    for(int p = 0; p < len; p++) {
        p2 = req->ptr + p;
        if(*(p2+1) == '\r' && *(p2+2) == '\n'){
            hlen = p2 - p1;           
            if(hlen > 0){

                name = p1;
                delim = strchr(name,':');
                if(delim){                                        
                    // note: trunkates header name and values
                    nlen = MIN(delim - name,MAX_HEADER_NAME - 1);
                    memcpy(headers->name, name, nlen);
                    headers->name[nlen] = 0;
                    stringToLowercase(headers->name,nlen);

                    value = delim + 1;                    
                    while(*value && (*value == ' ' || *value == '\t')) value++;
                    
                    if(*value) {
                        vlen = MIN(p2-value+1,MAX_HEADER_VALUE - 1);
                        memcpy(headers->value, value, vlen);
                        headers->value[vlen] = 0;
                    }
                    
                    headers++;
                    count++;

                    // reached headers limit
                    if(count >= maxh) return count; 
                }
            }
            p1 = p2+3;
            p+=2;
        }
    }
    
    return count;
}

int get_header_value(http_headers_t *headers, char *name, char **value){
    static int from = 0, to = sizeof(headers);
    
    *value = 0;
    
    for(int i = from; i < to; i++) {
        if(headers[i].name[0] == 0) break;
        
        if(strcmp(headers[i].name,name) == 0) { 
            *value = headers[i].value;
            return 1; // found
        }
    }
    
    return 0;
}

/* raw json error */
int bad_request(char **argv, int code, char *message){
    int status = code < 600 ? code : HTTP_400_BAD_REQUEST; 
    code = status != code ? code : 0;
    
    xbuf_t *reply = get_reply(argv);

    int *statusCodePtr = (int*)get_env(argv, HTTP_CODE);
    
    if(statusCodePtr)
        *statusCodePtr = status;
    
    xbuf_t err;
    xbuf_init(&err);
    xbuf_xcat(&err,
              "{\"status\":%d,\"error\":\"%s\",\"message\":\"%s\",\"code\":%d}",
              status, http_status(status), unquote(message), code);
    
    xbuf_t hdr;
    xbuf_init(&hdr);
    
    xbuf_xcat(&hdr,
              "HTTP/1.1 %s\r\n"
              "Content-type: application/json\r\n"
              "Content-Length: %d\r\n\r\n"
              "%s"
              ,http_status(status),err.len, err.ptr);
    
    xbuf_ncat(reply, hdr.ptr, hdr.len);
    
    xbuf_free(&hdr);
    xbuf_free(&err);
      
    return status;
}

int exec_endpoint(int argc, char **argv, EndpointEntry endpoints[], int count){
    xbuf_t *req = get_request(argv);
    xbuf_t *rep = get_reply(argv);
    
    db_t *db = db_session(argv);

    int method = get_env(argv, REQUEST_METHOD);
        
    char *cmd = argv[0];   
    EndpointEntry *entry = 0;    
    bool method_allowed = false;

    debug_printf("endpoint count %d\n", count);
    
    for(int index = 0; index < count; index++){
        entry = &endpoints[index];
        
        if(entry->method == method) {
            if(entry->cmd == 0 || (cmd && cmd[0] && strcmp(cmd,entry->cmd) == 0)){
                if(!db && !(entry->flags & EP_PERMIT_UNAUTH))
                    return db_error(rep,HTTP_401_UNAUTHORIZED,"unauthorized access");

                int http_error = entry->endpoint(argc,argv,req,rep,db); 

                if(!http_error){
                    http_error = db_response(db,rep,argv);                    
                }
                
                return http_error;
            }
            method_allowed = true;
        }
    }

    if(!method_allowed)
        return db_error(rep,HTTP_405_METHOD_NOT_ALLOWED,"method not allowed");

    return db_error(rep,HTTP_404_NOT_FOUND,"endpoint not found");
} 

