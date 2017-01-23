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

#define DEBUG
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>

#include "gwan.h"
#include "postgresql/libpq-fe.h"
#include "postrest/db.h"

static prnd_t rnd;
static int once = 0;



int session_get(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    
    if(argc < 2) return db_error(rep,400,"to few parameters");
    
    char *database = argv[0];
    char *user = argv[1];
    
    if(!strword(database)){
        db_error(rep,400,"[database] name must be a word <string>");
    }
    
    if(!strword(user)){
        db_error(rep,400,"[user] name must be a word <string>");
    }
    
    int session_id = get_env(argv, SESSION_ID);
    
    xbuf_xcat(rep,"{\"ssid\":\"%x\"}", session_id);
       
    return 200;
}


int session_login(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
 
    char *database;
    char *schema;
    char *username;
    char *password;
    
    // already in session
    if(db) {        
        return 200;
    }

    server_t *server = *((server_t**) get_env(argv, US_SERVER_DATA));

    if(!server){
        return db_error(rep,500,"undefined server data");        
    }
    
    char *host = (char *) kv_get(server->config,"dbhost",6);
    
    if(!host) {
        return db_error(rep,HTTP_502_BAD_GATEWAY,"missing database configuration");
    }
    
    char *entity = (char*)get_env(argv, REQ_ENTITY);
 
    if(!entity){
        return db_error(rep,400,"missing request entity");
    }
    
    jsn_t *json = jsn_frtext(entity,0);

    if(!json){            
        return db_error(rep,400,"failed to parse json");
    }
    
    jsn_t *user = jsn_byname(json,"username",1);

    if(!user || user->type != jsn_STRING){
        jsn_free(json);
        
        return db_error(rep,400,"[username] must be a <string>");
    }
    
    username = user->string;
    
    jsn_t *pass = jsn_byname(json,"password",1);
    
    if(!pass || pass->type != jsn_STRING){
        jsn_free(json);
        
        return db_error(rep,400,"[password] must be a <string>");
    }
    
    password = pass->string;
    
    jsn_t *dbname = jsn_byname(json,"database",1);

    if(dbname && (dbname->type != jsn_STRING || !strword(dbname->string))){
        jsn_free(json);
        
        return db_error(rep,400,"[database] name must be a word <string>");
    }
   
    database = dbname ? dbname->string : "postgres";
    
    jsn_t *sch = jsn_byname(json,"schema",1);

    if(sch && (sch->type != jsn_STRING || !strword(sch->string))){
        jsn_free(json);
        
        return db_error(rep,400,"[schema] name must be a word <string>");
    }
    
    schema = sch ? sch->string : "public";

    char connstr[128] = {};
    char session_key[128] = {};

    char *dbconn = (char *) kv_get(server->config,"dbconn",6);

    // todo: get connection url from global configuration
    s_snprintf(connstr,sizeof(connstr)-1, dbconn, 
        username, password, host, database);

    
    // note: this is a blocking / synchronous request
    PGconn *conn = PQconnectdb(connstr);

    int status = PQstatus(conn);
    
    if (status == CONNECTION_OK && PQsetnonblocking(conn, 1) == 0)
    {
        http_headers_t headers[17] = {};

	get_headers(req,headers,16);
	char *remote = 0;
	get_header_value(headers,"X-Forwarded-For",&remote);

	if(remote){
	    debug_printf("remote ip: %s\n", remote);
	}

	debug_printf("client ip: %s\n",argv[-1]);
	// note: only ipv4
	struct sockaddr_in sa;
	int s = inet_pton(AF_INET, (remote ? remote : argv[-1]), &(sa.sin_addr));
	if (s <= 0) {
	    if (s == 0) return db_error(rep,400,"x-forwarded-for invalid address");
	    else return db_error(rep,500,0);
	}

	debug_printf("sin_addr %X\n",sa.sin_addr.s_addr);

	if(!once) {
	    once = 1;
	    debug_printf("sw_init(): once\n");
	    sw_init(&rnd, (u32)getns());
	}
	
	// random+ipv4ip
	u64 sid = ((u64)sw_rand(&rnd) << 32) + sa.sin_addr.s_addr;
	debug_printf("sid(%lX)\n",sid);
	
        // username@database:sid
        s_snprintf(session_key,sizeof(session_key)-1,"%s@%s:%llX", username, database, sid);
         
        db = calloc(1,sizeof(db_t));
        db->conn = conn;
        db->skey = strdup(session_key);
        db->name = PQdb(conn);
        db->user = PQuser(conn);
        db->host = PQhost(conn);
        db->port = PQport(conn);
        db->options = PQoptions(conn);
        db->schema = strdup(schema);
        db->timestamp = getms();
        db->timeout = db->timestamp + DB_SESSION_TIMEOUT;
        
        // store into user session
        kv_add(server->sessions, &(kv_item){
            .key = db->skey,
            .klen = strlen(db->skey),
            .val = (void *) db,
            .flags = 0
        });
        
        debug_printf("created session_key %s\n", db->skey);
    } else {
        char *errmsg = PQerrorMessage(conn); 
        
        debug_printf("connection failed: %s\n", errmsg);
        
        bad_request(argv,HTTP_401_UNAUTHORIZED, errmsg);
        
        PQfinish(conn);
        
        jsn_free(json);
    
        return 401;
    }
    
    jsn_free(json);
    
    xbuf_xcat(rep,"{\"authorization\":\"Basic %B\"}", db->skey);
    
    return HTTP_230_AUTHENTICATION_SUCCESSFUL;
}


int session_logout(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    
    xbuf_xcat(rep,"{\"authorization\":null}");
    
    db_free(argv,db);
    
    return HTTP_200_OK;
}


int main(int argc, char **argv){
    EndpointEntry endpoints[] = {
        {HTTP_GET, 0, session_get, EP_PERMIT_UNAUTH},
        {HTTP_POST, 0, session_login, EP_PERMIT_UNAUTH},
        {HTTP_DELETE, 0, session_logout, 0},
    };
    
    return exec_endpoint(argc,argv,endpoints,ArrayCount(endpoints));
}
