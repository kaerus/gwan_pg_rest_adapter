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
#include <stdlib.h>

#include "gwan.h"
#include "postrest/db.h"

/* GET _document/byid/:collection/:uuid
 *
 */
int get_document(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {

    char *collection = 0;
    
    if(argc > 1){
        collection = argv[1];
    }
    
    if(!strword(collection)) {
        return db_error(rep,422,"[collection] name must be a word <string>");
    }
    
    char *uuid = 0;
    
    if(argc > 2) {
        uuid = argv[2];
    }
    
    if(!strword(uuid)){
        return db_error(rep,422,"document [id] must be an uuid <string>");
    }

    xbuf_t query;
    xbuf_init(&query);

    xbuf_xcat(&query,
        "SELECT id, name, xmin AS rev, document AS json_doc "
        "FROM %s.%s.%s_collection where id = '%s';"
        ,db->name, db->schema, collection, uuid);
    
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);
    
    return ret;
}

/* GET _document/byname/:collection/:name
 *
 */
int getbyname_document(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {

    char *collection = 0;
    
    if(argc > 1){
        collection = argv[1];
    }
    
    if(!strword(collection)) {
        return db_error(rep,422,"[collection] name must be a word <string>");
    }
    
    char *name = 0;
    
    if(argc > 2) {
        name = argv[2];
    }
    
    if(!isString(name)){
        return db_error(rep,422,"document [name] must be a <string>");
    }
    
    char decoded[256];
    
    debug_printf("encoded name: %s\n",name);
    url_decode(decoded,name);
    debug_printf("decoded name: %s\n",decoded);

    if(!isString(decoded)){
        return db_error(rep,422,"document [name] must be a <string>");
    }
    
    xbuf_t query;
    xbuf_init(&query);

    xbuf_xcat(&query,
	      "SELECT id, name, xmin AS rev, document AS json_doc "
	      "FROM %s.%s.%s_collection where name = '%s';"
	      ,db->name, db->schema, collection, decoded);
    
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);
    
    return ret;
}

/* GET _document/all/:collection/:limit/:offset
 *
 */
int all_document(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    int limit = 0;
    int offset = 0;
    char *collection = 0;
    
    if(argc > 1){
        collection = argv[1];
    }
    
    if(!strword(collection)) {
        return db_error(rep,422,"[collection] name must be a word <string>");
    }
    
    if(argc > 2){
        limit = atoi(argv[2]);
    }
    if(argc > 3){
        offset = atoi(argv[3]);
    }
    
    xbuf_t query;
    xbuf_init(&query);

    xbuf_xcat(&query,
        "SELECT id, name, xmin AS rev, document AS json_doc "
        "FROM %s.%s.%s_collection"
        ,db->name, db->schema, collection);
    
    if(limit) xbuf_xcat(&query," LIMIT %u", limit); 
    if(offset) xbuf_xcat(&query," OFFSET %u", offset); 
    
    xbuf_xcat(&query,";");
    
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);
    
    return ret;
}

/* GET _document/list/:collection
 *
 */
int list_document(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    int limit = 0;
    int offset = 0;
    char *collection = 0;
    
    if(argc > 1){
        collection = argv[1];
    }
    
    if(!strword(collection)) {
        return db_error(rep,422,"[collection] name must be a word <string>");
    }
    
    if(argc > 2){
        limit = atoi(argv[2]);
    }
    if(argc > 3){
        offset = atoi(argv[3]);
    }
    
    xbuf_t query;
    xbuf_init(&query);

    xbuf_xcat(&query,
        "SELECT id, name, xmin AS rev "
        "FROM %s.%s.%s_collection"
        ,db->name, db->schema, collection);
    
    if(limit) xbuf_xcat(&query," LIMIT %u", limit); 
    if(offset) xbuf_xcat(&query," OFFSET %u", offset); 
    
    xbuf_xcat(&query,";");
      
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);
    
    return ret;
}


/* POST _document/find/:collection
 * entity: json { <object> }
 */
int find_document(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    int limit = 0;
    int offset = 0;
    char *collection = 0;
    
    if(argc > 1){
        collection = argv[1];
    }
    
    if(!strword(collection)) {
        return db_error(rep,422,"[collection] name must be a word <string>");
    }
    
    if(argc > 2){
        limit = atoi(argv[2]);
    }
    
    if(argc > 3){
        offset = atoi(argv[3]);
    }
    
    char *entity = (char*)get_env(argv, REQ_ENTITY);
    
    
    jsn_t *json = jsn_frtext(entity,0);

    if(!json) {
        return db_error(rep, 400,"failed to parse json");
    }
    
    xbuf_t query;
    xbuf_t txt;
    xbuf_init(&query);
    xbuf_init(&txt);

    xbuf_xcat(&query,
        "SELECT id, name, xmin AS rev, document AS json_doc FROM %s.%s.%s_collection "
        "WHERE document @> '%s'"
        ,db->name, db->schema, collection, jsn_totext(&txt,json,0));
    
    if(limit) xbuf_xcat(&query," LIMIT %u", limit); 
    if(offset) xbuf_xcat(&query," OFFSET %u", offset); 
    
    xbuf_xcat(&query,";");
    
    xbuf_free(&txt);

    debug_printf("find: %s\n",query.ptr);
    
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);  
    jsn_free(json);
   
    return ret;
}

/* POST _document/create/:collection
 * entity: json { <object> }
 */
int create_document(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    char *collection = 0;
    
    if(argc > 1){
        collection = argv[1];
    }

    if(!strword(collection)) {
        return db_error(rep,422,"[collection] name must be a word <string>");
    }
    
    char *entity = (char*)get_env(argv, REQ_ENTITY);
    
    jsn_t *json = jsn_frtext(entity,0);

    if(!json) {
        return db_error(rep, 400,"failed to parse json");
    }

    jsn_t *document = jsn_byname(json,"document",1);

    jsn_t *name = 0;
    
    if(document){
	if(document->type != jsn_NODE){
	    return db_error(rep,400,"[document] must be <json>");
	}

	name = jsn_byname(json,"name",1);

	if(name && name->type != jsn_STRING){
	    return db_error(rep,400,"[name] must be a <string>");
	}
    } else {
	document = jsn_byname(json,"documents",1);

	if(!document || document->type != jsn_ARRAY){
	    return db_error(rep,400,"[documents] must be <array>");
	}

	jsn_t *item = jsn_byindex(document,0);

	// guard with limit on items
	while(item){
	    if(item->type != jsn_NODE){
		return db_error(rep,400,"[documents] items must be <object>");
	    }
	    item = item->next;
	}
    }

    xbuf_t query;
    xbuf_t txt;
    xbuf_init(&query);
    xbuf_init(&txt);
    if(name) {
	// fixme: possible SQL injection on name
	xbuf_xcat(&query,
		  "INSERT INTO %s.%s.%s_collection(name,document) VALUES ('%s','%s') "
		  ,db->name, db->schema, collection, name->string, jsn_totext(&txt,document,0));
    } else {
	// insert multiple documents
	if(document->type == jsn_ARRAY) {
	    xbuf_xcat(&query,
		      "INSERT INTO %s.%s.%s_collection(name,document) VALUES "
		      ,db->name, db->schema, collection);
		    
	    jsn_t *item = jsn_byindex(document,0);
	    jsn_t *name;
	    jsn_t *doc;
	    while(item){
		name = jsn_byname(item,"_name",1);

		if(!name) doc = item;
		else doc = jsn_byname(item,"_json",1);
		
		if(name && name->string) {
		    xbuf_xcat(&query,"('%s','%s')"
			      ,name->string, jsn_totext(&txt,doc,0));
		} else {
		    xbuf_xcat(&query,"(NULL,'%s')"
			      ,jsn_totext(&txt,doc,0));
		}
		
		if(item->next) xbuf_xcat(&query,",");

		item = item->next;
		xbuf_reset(&txt);
	    }
	} else {
	    // insert single document
	    xbuf_xcat(&query,
		      "INSERT INTO %s.%s.%s_collection(document) VALUES('%s') "	      
		      ,db->name, db->schema, collection, jsn_totext(&txt,document,0));
	}
    }

    xbuf_xcat(&query,"RETURNING id, name, xmin AS rev;");
    
    xbuf_free(&txt);

    int ret = db_send(db,rep,query.ptr);
    
    debug_printf("create document\n");
    xbuf_free(&query);  
    jsn_free(json);

    return ret;
}

/* POST _document/set/:collection/:uuid
 * entity: json { <object> }
 */
int set_document(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {    
    char *collection = 0;
    
    if(argc > 1){
        collection = argv[1];
    }
    
    if(!strword(collection)) {
        return db_error(rep,422,"[collection] name must be a word <string>");
    }
    
    char *uuid = 0;
    
    if(argc > 2){
        uuid = argv[2];
    }
    
    if(!strword(uuid)){
        return db_error(rep,422,"document [id] must be an uuid <string>");
    }

    char *entity = (char*)get_env(argv, REQ_ENTITY);

    jsn_t *json = jsn_frtext(entity,0);
	
    if(!json) {
        return db_error(rep, 400,"failed to parse json");
    }

    jsn_t *name = jsn_byname(json,"name",1);

    if(name && name->type != jsn_STRING){
	return db_error(rep,400,"[name] must be a <string>");
    }
    
    jsn_t *document = jsn_byname(json,"document",1);

    if(!name && !document){
	return db_error(rep,400,"must update at least [name] or [document]");
    }
    
    if(document && document->type != jsn_NODE){
	return db_error(rep,400,"[document] must be <json>");
    }
    
    xbuf_t query;
    xbuf_t txt;
    xbuf_init(&query);
    xbuf_init(&txt);

    if(name){
	if(document){
	    // update name and document
	    xbuf_xcat(&query,
		      "UPDATE %s.%s.%s_collection SET (name, document) = ('%s') "
		      "WHERE id = '%s' "
		      "RETURNING id, xmin AS rev;"
		      ,db->name, db->schema, collection, name->string, jsn_totext(&txt,document,0), uuid);
	} else {
	    // update name only
	    xbuf_xcat(&query,
		      "UPDATE %s.%s.%s_collection SET (name) = ('%s') "
		      "WHERE id = '%s' "
		      "RETURNING id, xmin AS rev;"
		      ,db->name, db->schema, collection, name->string, uuid);

	}
    } else {
	// update document only
	xbuf_xcat(&query,
		  "UPDATE %s.%s.%s_collection SET (document) = ('%s') "
		  "WHERE id = '%s' "
		  "RETURNING id, xmin AS rev;"
		  ,db->name, db->schema, collection, jsn_totext(&txt,document,0), uuid);
	
    }
    
    xbuf_free(&txt);
    
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);  
    jsn_free(json);
    
    return ret;
}

/* PUT _document/:collection/:uuid
 * entity: json { <object> }
 */
int update_document(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    char *collection = 0;
    
    if(argc > 0){
        collection = argv[0];
    }
    
    if(!strword(collection)) {
        return db_error(rep,422,"[collection] name must be a word <string>");
    }
    
    char *uuid = 0;
    
    if(argc > 1){
        uuid = argv[1];
    }
    
    if(!strword(uuid)){
        return db_error(rep,422,"document [id] must be an uuid <string>");
    }

    char *entity = (char*)get_env(argv, REQ_ENTITY);
    
    jsn_t *json = jsn_frtext(entity,0);

    if(!json) {
        debug_printf("json body: %s",entity);
        return db_error(rep, 400,"failed to parse json");
    }

    jsn_t *name = jsn_byname(json,"name",1);

    if(name && name->type != jsn_STRING){
	return db_error(rep,400,"[name] must be a <string>");
    }
    
    jsn_t *document = jsn_byname(json,"document",1);

    if(!name && !document){
	return db_error(rep,400,"must update at least [name] or [document]");
    }
    
    if(document && document->type != jsn_NODE){
	return db_error(rep,400,"[document] must be <json>");
    }
    
    xbuf_t query;
    xbuf_t txt;
    xbuf_init(&query);
    xbuf_init(&txt);

    if(name){
	if(document){
	    // update name and document
	    xbuf_xcat(&query,
		      "UPDATE %s.%s.%s_collection x SET (name,document) = "
                      "('%s',jsonb_strip_nulls((select document FROM %s.%s.%s_collection y "
                      "WHERE y.id = x.id)::jsonb || '%s'::jsonb)) "
                      "WHERE x.id = '%s' "
		      "RETURNING id, xmin AS rev;"
		      ,db->name, db->schema, collection
                      ,db->name, db->schema, collection
                      ,name->string, jsn_totext(&txt,document,0), uuid);
	} else {
	    // update name only
	    xbuf_xcat(&query,
		      "UPDATE %s.%s.%s_collection SET (name) = ('%s') "
		      "WHERE id = '%s' "
		      "RETURNING id, xmin AS rev;"
		      ,db->name, db->schema, collection, name->string, uuid);

	}
    } else {
	// update document only
	xbuf_xcat(&query,
                  "UPDATE %s.%s.%s_collection x SET (document) = "
                  "(jsonb_strip_nulls((select document FROM %s.%s.%s_collection y "
                  "WHERE y.id = x.id)::jsonb || '%s'::jsonb)) "
                  "WHERE x.id = '%s' "
                  "RETURNING id, xmin AS rev;"
                  ,db->name, db->schema, collection
                  ,db->name, db->schema, collection
                  ,jsn_totext(&txt,document,0), uuid);
	
    }
    
    xbuf_free(&txt);
    
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);  
    jsn_free(json);
    
    return ret;
}

/* DELETE _document/:collection/:uuid
 * 
 */
int delete_document(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    char *collection = 0;
    
    if(argc > 0){
        collection = argv[0];
    }
    
    if(!strword(collection)) {
        return db_error(rep,422,"[collection] name must be a word <string>");
    }
    
    char *uuid = 0;
    
    if(argc > 1){
        uuid = argv[1];
    }
    
    if(!strword(uuid)){
        return db_error(rep,422,"document [id] must be an uuid <string>");
    }
    
    xbuf_t query;
    xbuf_init(&query);
    
    xbuf_xcat(&query,
        "DELETE FROM %s.%s.%s_collection WHERE id = '%s' "
        "RETURNING id, xmin AS rev;"
        , db->name, db->schema, collection, uuid);

    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);  
    
    return ret;
}


int main(int argc, char **argv){
    EndpointEntry endpoints[] = {
        {HTTP_GET, "get", get_document, 0},
        {HTTP_GET, "name", getbyname_document, 0},
        {HTTP_GET, "list", list_document, 0}, 
        {HTTP_GET, "all", all_document, 0},
        {HTTP_PUT, 0, update_document, 0},
        {HTTP_POST, "create", create_document, 0},
        {HTTP_POST, "find", find_document, 0},
        {HTTP_POST, "set", set_document, 0},
        {HTTP_DELETE, 0, delete_document, 0},
    };

    return exec_endpoint(argc,argv,endpoints,ArrayCount(endpoints));
}
