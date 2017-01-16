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

#include <string.h>

#include "gwan.h"
#include "postrest/db.h"

int get_index(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    return HTTP_501_NOT_IMPLEMENTED;
}

int update_index(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    return HTTP_501_NOT_IMPLEMENTED;
}

int delete_index(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    char *collection = argv[0];
    
    if(!strword(collection)) {
        return db_error(rep,422,"[collection] name must be a word <string>");
    }
    
    char *field = argv[1];
    
    if(!strword(field)) {
        return db_error(rep,422,"[index] name must be a word <string>");
    }
    
    char *drop_method = "";
    char *entity = (char*)get_env(argv, REQ_ENTITY);
    
    if(entity) {
        jsn_t *json = jsn_frtext(entity,0);

        if(!json){
            
            return db_error(rep,400,"failed to parse json");
        }
        
        jsn_t *attr = jsn_byname(json,"cascade",1);
    
        if(attr && attr->type == jsn_TRUE){
            drop_method = "CASCADE";
        }
        
        attr = jsn_byname(json,"restrict",1);
    
        if(attr && attr->type == jsn_TRUE){
            drop_method = "RESTRICT";
        }
        
        jsn_free(json);
    }
    
    char idx_name[128];
    
    s_snprintf(idx_name,sizeof(idx_name)-1, "%s_%s_%s_%s_idx",db->name,db->schema,collection,field);
    
    xbuf_t query;
    xbuf_init(&query);
    // DROP INDEX [ IF EXISTS ] name [, ...] [ CASCADE | RESTRICT ]
    xbuf_xcat(&query,
        "DROP INDEX IF EXISTS %s %s;"
        ,idx_name, drop_method);
    
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);
    
    return ret ? ret : db_response(db,rep,argv);
}

int create_index(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    /* create document collection index
        {
            "collection": "<name>",
            "field": "<field>"
        }
    */
    char *collection = argv[0];
    
    if(!strword(collection)) {
        return db_error(rep,422,"[collection] name must be a word <string>");
    }
    
    
    char *entity = (char*)get_env(argv, REQ_ENTITY);
    
    jsn_t *json = jsn_frtext(entity,0);

    if(!json) {
        return db_error(rep, 400,"failed to parse json");
    }
    
    jsn_t *field = jsn_byname(json,"field",1);
    
    if(!field || field->type != jsn_STRING || !strword(field->string)){
        jsn_free(json);
        ReplyJsError(400,"[field] must be a word <string>");
        
        return 400;
    }
    
    char idx_name[128];
    
    s_snprintf(idx_name,sizeof(idx_name)-1, "%s_%s_%s_%s_idx",db->name,db->schema,collection,field);
    
    xbuf_t query;
    xbuf_init(&query);
    xbuf_xcat(&query,
        "CREATE INDEX CONCURRENTLY %s ON %s.%s.%s_collection USING GIN ((document->'%s'));"
        ,idx_name, db->name, db->schema, collection, field->string);
    
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);
    jsn_free(json);
    
    return ret ? ret : db_response(db,rep,argv);
}

int main(int argc, char **argv){
    int method = get_env(argv, REQUEST_METHOD);
    xbuf_t *req = get_request(argv);
    xbuf_t *rep = get_reply(argv);
    db_t *db = db_session(argv);
    
    if(!db) {
        return db_error(rep,HTTP_401_UNAUTHORIZED,"authentication failed");
    }

    char *q = argv[0];
            
    if(!q || q[0] == 0) {
        return db_error(rep,HTTP_400_BAD_REQUEST,"no such command");
    }
    
    switch(method) {
        case HTTP_GET: {            
           return get_index(argc,argv,req,rep,db);
        } break;
        case HTTP_POST: {
            return create_index(argc,argv,req,rep,db);
        } break;
        case HTTP_DELETE: {
            return delete_index(argc,argv,req,rep,db);
        } break;
        case HTTP_PUT: {
            return update_index(argc,argv,req,rep,db);
        } break;
        default: break;
    }
    
    return db_error(rep,HTTP_405_METHOD_NOT_ALLOWED,"method not allowed");
}
