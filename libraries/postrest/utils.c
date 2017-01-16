

/* removes double quotes and control codes from string */
char *unquote(char *string){
    int length = strlen(string);
    
    for(int i = 0; i < length; i++){
        if(string[i] == '"') string[i] = 0x27; // replace with a single quote
        else if(string[i] < 0x20) string[i] = '.'; // replace control codes with a dot.
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

// validate uuid string
// aa196922-4471-4c1f-aa4a-f8d7bdb39bff
int isUUID(char *string){
    if(!string) return 0;

    int len = strlen(string);
    
    if(len != 36) return 0;

    char c;
    
    for(int i = 0; i < len; i++){
	c = string[i];
	if(c != 0x2d &&
	   ((c < 0x30 || c > 0x66) ||
	    (c > 0x39 && c < 0x61))) {
	    return 0;
	}
    }

    return 1;
}

/* check that string is a valid csv list */
int strcsv(char *string){
    char c;
    
    for(int i = 0; i < strlen(string); i++){
        c = string[i];
        
        if(c < 0x2c || c > 0x7a || (c > 0x3a && c < 0x40) || (c > 0x5a && c < 0x61)) {
            return 0;
        }
    }
    
    return 1;
}

int strdigit(char *string){
    char c;
    
    for(int i = 0; i < strlen(string); i++){
        c = string[i];
        
        if(c < 0x30 || c > 0x39) {
            return 0;
        }
    }

    return 1;
}



xbuf_t *get_request(char **argv) {
    return (xbuf_t *) get_env(argv,READ_XBUF);
}

/* parse and truncate http headers */
int get_headers(xbuf_t *req, http_headers_t *headers, int maxh){
    char header[257] = {0};
    char *name, *value, *delim;
    
    u32 len = req->len;
    char c1, c2, *s1, *s2;
    
    int count = 0, hlen;
    
    s1 = req->ptr;
   
    for(int p = 0; p < len; p++) {
        s2 = req->ptr + p;
        c1 = *s2;
        c2 = *(s2 + 1);
        if(c1 == '\r' && c2 == '\n'){
            if(maxh == 0) return -1; // reached headers limit
            hlen = s2 - s1;
            
            if(hlen <= 0 || count > 15) break;
            if(hlen > sizeof(header) - 2) hlen = 255;
            
            strncpy(header,s1,hlen);
            header[hlen] = 0;
            
            name = header;
            delim  = strchr(header,':');
            if(delim){
                header[delim-name] = 0;
                value = delim + 1;
                if(*value == ' ') value++;
                // todo: should we bail out on to long header name/value?
                strncpy(headers->name, name, sizeof(headers->name)-1);
                strncpy(headers->value, value, sizeof(headers->value)-1);
                headers++;
                maxh--;
            }
            count++;
            s1 = s2 + 2;
        }
    }
    
    return 0;
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
