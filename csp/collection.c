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
#include "gwan.h"
#include "postrest/db.h"


int get_collection(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    return HTTP_501_NOT_IMPLEMENTED;
}

int put_collection(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    return HTTP_501_NOT_IMPLEMENTED;
}

int delete_collection(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    char *collection = 0;

    if(argc > 0){
        collection = argv[0];
    }
    
    if(!strword(collection)) {
        return db_error(rep,422,"[collection] name must be a word <string>");
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
    
    xbuf_t buf, *query = &buf;
    xbuf_init(query);
    xbuf_xcat(query,"DROP TABLE IF EXISTS %s.%s.%s_document_collection %s;"
        ,db->name, db->schema, collection, drop_method);
    
    int ret = db_send(db,rep,query->ptr);
    
    xbuf_free(query);
    
    return ret ? ret : db_response(db,rep,argv);
}

/* POST /_collection/:collection/
   {
   "index": "<index type>" 
   }
*/
int create_collection(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    char *collection = 0;

    if(argc > 0){
        collection = argv[0];
    }
    
    if(!strword(collection)) {
        return db_error(rep,422,"[collection] name must be a word <string>");
    }
    
    debug_printf("creating database %s collection %s\n", db->name, collection);
    
    char *index = "jsonb_path_ops";

    char *entity = (char*)get_env(argv, REQ_ENTITY);
    
    if(entity) {
        jsn_t *json = jsn_frtext(entity,0);

        if(!json){            
            return db_error(rep,400,"failed to parse json");
        }
        
        jsn_t *idx = jsn_byname(json,"index",1);
    
        if(idx && (idx->type != jsn_STRING || !strword(idx->string))){
            jsn_free(json);
            
            return db_error(rep,400,"[index] must be a word <string>");
        }
        
        if(idx) index = idx->string;
        
        jsn_free(json);
    }
    
    xbuf_t query;
    xbuf_init(&query);
    xbuf_xcat(&query,
	      "CREATE TABLE IF NOT EXISTS %s.%s.%s_document_collection "
	      "(id uuid PRIMARY KEY DEFAULT gen_random_uuid(), "
	      "name text, "
	      "document jsonb NOT NULL);"
	      ,db->name, db->schema, collection);
    
    char idx_name[128];

    // index name suffix
    s_snprintf(idx_name,sizeof(idx_name)-1, "%s_%s_%s_idx",db->name,db->schema,collection);

    // create GIN index on jsonb document 
    xbuf_xcat(&query,
	      "CREATE INDEX document_%s ON %s.%s.%s_document_collection USING GIN (document %s);"
	      ,idx_name, db->name, db->schema, collection, index);

    // create partial index on name 
    xbuf_xcat(&query,
	      "CREATE INDEX name_%s ON %s.%s.%s_document_collection(name) WHERE name IS NOT NULL;"
    	      ,idx_name, db->name, db->schema, collection, index);
    
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);
    
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
           return get_collection(argc,argv,req,rep,db);
        } break;
        case HTTP_POST: {
            return create_collection(argc,argv,req,rep,db);
        } break;
        case HTTP_DELETE: {
            return delete_collection(argc,argv,req,rep,db);
        } break;
        case HTTP_PUT: {
            return put_collection(argc,argv,req,rep,db);
        } break;
        default: break;
    }
    
    return db_error(rep,HTTP_405_METHOD_NOT_ALLOWED,"method not allowed");
}