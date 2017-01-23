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
#include "gwan.h"
#include "postrest/db.h"


int get_collection(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    return HTTP_501_NOT_IMPLEMENTED;
}

int update_collection(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
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
    
    return ret;
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
    
    return ret;
}

int main(int argc, char **argv){
    EndpointEntry endpoints[] = {
        {HTTP_GET, 0, get_collection, 0},
        {HTTP_PUT, 0, update_collection, 0},
        {HTTP_POST, 0, create_collection, 0},
        {HTTP_DELETE, 0, delete_collection, 0},
    };

    return exec_endpoint(argc,argv,endpoints,ArrayCount(endpoints));
}

