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
#pragma link "libraries/postrest/db.c"
#pragma link "libraries/libpq/libpq.so"
#pragma debug
#define DEBUG

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include <errno.h>
#include <assert.h>

#include "gwan.h"
#include "postrest/libpq-fe.h"
#include "postrest/db.h"

int
init(int argc, char *argv[]){

    debug_printf("Handler Init\n");
  
    kv_t **kv = (kv_t**) get_env(argv, US_SERVER_DATA);
     
    // initialize sessions 
    if(!*kv) {
        *kv = (kv_t *)calloc(1,sizeof(kv_t));
        
        if(!*kv) {
            return -1;
        }
        kv_init(*kv, "session", 0, 0, 0, 0);
        debug_printf("initialized session kv store\n");
    }
    
    // store database host 
    char **db_host = (char **) get_env(argv, US_HANDLER_DATA);
    
    assert(db_host);
    
    if(!*db_host) {
        *db_host = strdup("127.0.0.1:5432");
    }

    u32 *states = (u32 *) get_env(argv, US_HANDLER_STATES);
    *states = 0xFFFFFFFF;/*(1L << HDL_AFTER_ACCEPT) |
			   (1L << HDL_AFTER_READ) |
			   (1L << HDL_BEFORE_PARSE) |
			   (1L << HDL_AFTER_PARSE);
			 */
    return 0;
}

void clean(int argc, char *argv[]){

    debug_printf("Handler clean()\n");
    kv_t **kv = (kv_t**) get_env(argv, US_SERVER_DATA);
    
    if(kv) {
        /* todo: free stray session data */
        free(*kv);
    }
    
    char **db_host = (char **) get_env(argv, US_VHOST_DATA);
    
    if(db_host) {
        free(*db_host);
    }
}

int main(int argc, char *argv[])
{

    long state = (long)argv[0];

    switch(state){
    case HDL_AFTER_ACCEPT:{
	char *szIP = (char*)get_env(argv, REMOTE_ADDR);
	// setup small garbage collected memory buffer
	gc_init(argv,2048); 
            
	debug_printf("--handler client accept: %s\n",szIP);

	// display session id and client port
	int session = get_env(argv,SESSION_ID);
	int port = get_env(argv,REMOTE_PORT);
            
           
	debug_printf("session id %x port %u\n", session, port);
    } break;
    case HDL_AFTER_READ: {
	char *szRequest = (char*)get_env(argv, REQUEST);
	debug_printf("--handler client request: %s\n", szRequest);

	if(strncmp(szRequest, "GET / ", 6) == 0) {
	    xbuf_t *rep = get_reply(argv); 
	    xbuf_xcat(rep, 
		      "HTTP/1.1 301 Moved Permanently\r\n"
		      "Content-type: text/html\r\n"
		      "Location: index.html\r\n\r\n"
		      "<html><head><title>Redirect</title></head>"
		      "<body>Click <a href=\"index.html\">here</a>"
		      ".</body></html>");
	    return 2; 
	}
    } break;
    case HDL_BEFORE_PARSE: {
	debug_printf("before parse\n");
    } break;
    case HDL_AFTER_PARSE: {
	char *szAgent = (char*)get_env(argv, USER_AGENT);
	debug_printf("--handler user agent: %s\n", szAgent);

	char *api = (char *) get_env(argv,QUERY_STRING);

	if(api) {
	    // switch to json reply format
	    char *mime = (char*) get_env(argv,REPLY_MIME_TYPE);
	    mime[0] = '.'; mime[1] = 'j'; mime[2] = 's';
	    mime[3] = 'o'; mime[4] = 'n'; mime[5] = 0;
			
	    db_t *db = db_session(argv);
	    if(!db){
		xbuf_t *req = get_request(argv);
		if( strcmp(req->ptr,"POST /?session") != 0){
		    debug_printf("req-ptr:%s\n", req->ptr);
		    bad_request(argv,HTTP_401_UNAUTHORIZED,"not authenticated");
                
		    return 2;
		}
	    }
	}
    } break;
    case HDL_BEFORE_WRITE: {
	xbuf_t *reply = get_reply(argv);
            
	debug_printf("reply len(%d) content(%s)\n",reply->len, reply->ptr);
    } break;
    case HDL_AFTER_WRITE: {
            
    } break;
    case HDL_BEFORE_CLOSE: {
	/* close session */
	debug_printf("closing session %x\n",(int) get_env(argv,SESSION_ID));
    } break;
    default: break;
    }
    
    return(255); 
}
