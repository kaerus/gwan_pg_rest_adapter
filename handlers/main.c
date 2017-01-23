/* 
 * Copyright (c) 2015 Kaerus Software AB, all rights reserved.
 * Author Anders Elo <anders @ kaerus com>.
 *
 * Licensed under Apache 2.0 Software License terms, (the "License");
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma link "libraries/postrest/db.c"
#pragma link "pq"
#pragma link "event"

#pragma debug
#define DEBUG

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include <errno.h>
#include <assert.h>

#include <event2/event.h>

#include "gwan.h"
#include "postrest/libpq-fe.h"
#include "postrest/db.h"


int
init(int argc, char *argv[]){

    debug_printf("Handler Init\n");
  
    server_t *server = *((server_t**) get_env(argv, US_SERVER_DATA));

    if(!server) {
        printf("undefined US_SERVER_DATA, aborting.");
        exit(-1);
    }

    if(!server->sessions) {
        printf("undefined sesssion data, aborting.");
        exit(-2);
    }

    if(!server->config) {
        printf("undefined config data, aborting.");
        exit(-3);        
    }
       
    u32 *states = (u32 *) get_env(argv, US_HANDLER_STATES);
    *states = 0xFFFFFFFF;/*(1L << HDL_AFTER_ACCEPT) |
			   (1L << HDL_AFTER_READ) |
			   (1L << HDL_BEFORE_PARSE) |
			   (1L << HDL_AFTER_PARSE);
			 */ 

    struct timeval sessions_timer = {1,0};    
    struct event_base *base = event_base_new();
    struct event *ev;

    kv_add(server->config,&(kv_item){
            .key = strdup("evbase"),
                .klen = 6,
                .val = (void *) base,
                .flags = 0
                });
    ev = event_new(base, -1, EV_PERSIST, db_timer_cb, server);
    event_add(ev, &sessions_timer);

    event_base_loop(base, EVLOOP_NONBLOCK);
    
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
	gc_init(argv,4096); 
            
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
        debug_printf("request accepted\n");
    } break;
    case HDL_BEFORE_PARSE: {
	debug_printf("before parse\n");
    } break;
    case HDL_AFTER_PARSE: {
	char *szAgent = (char*)get_env(argv, USER_AGENT);
	debug_printf("--handler user agent: %s\n", szAgent);

	char *api = (char *) get_env(argv,QUERY_STRING);
	debug_printf("--query string: %s\n", api);
	if(api) {
	    // switch to json reply format
	    char *mime = (char*) get_env(argv,REPLY_MIME_TYPE);
	    mime[0] = '.'; mime[1] = 'j'; mime[2] = 's';
	    mime[3] = 'o'; mime[4] = 'n'; mime[5] = 0;

            debug_printf("dispatching to api endpoint\n");
            /*		
	    db_t *db = db_session(argv);
	    if(!db){
		xbuf_t *req = get_request(argv);
		//if( strcmp(req->ptr,"POST /?session") != 0){
                if( strcmp(req->ptr,"GET /?oauth") != 0) {
		    debug_printf("req-ptr:%s\n", req->ptr);
		    bad_request(argv,HTTP_401_UNAUTHORIZED,"not authenticated");
                
		    return 2;
		}
	    }
            */
	}
    } break;
    case HDL_BEFORE_WRITE: {
        xbuf_t *reply = get_reply(argv);               

	debug_printf("--reply len(%d) content(%s)\n",reply->len, reply->ptr);
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
