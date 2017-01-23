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
 
/* SQL query API, use with caution since it allows anything */ 
/* Disable the servlet by removing this source file...      */

#pragma link "libraries/postrest/db.c"
#pragma link "pq"
#pragma link "event"

#include <string.h>
#include <stdlib.h>

#include "gwan.h"
#include "postrest/db.h"

int get_query(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t * db) {
    char *table = argv[0];

    u32 x_offset = 0;
    u32 x_limit = 0;
  
    char *offset = 0;
    char *limit = 0;
    char *orderby = 0;
    
    // parse url options
    get_arg("-offset=", &offset, argc, argv);
    
    if(offset && !strdigit(offset)){
        return db_error(rep,422,"[-offset] must be a <number>");
    }
    
    get_arg("-limit=", &limit, argc, argv);
    
    if(limit && !strdigit(limit)){
        return db_error(rep,422,"[-limit] must be a <number>");
    }
    
    get_arg("-order=", &orderby, argc, argv);
    
    if(orderby && !strword(orderby)){
        return db_error(rep,422,"[-orderby must be a word <string>");
    }

    // build SQL string for simple queries
    // note: use PG views or POST to handle more complex queries 
    xbuf_t query;
    xbuf_init(&query);
    
    xbuf_xcat(&query,"SELECT row_to_json(t) from %s.%s.%s as t",db->name,db->schema,table);

    // WHERE x <expression> AND y <expression>
    // example: id=3, id<3, number<>3, data->>'author'->>'name'='Charles'
    if(argc > 1) {
        int p = 1;
        
        while(argc > p) {
            // ignore -options
            if(argv[p][0] != '-') {
                xbuf_xcat(&query," %s t.%s", p == 1 ? "WHERE" : "AND", argv[p]);
            }
            
            p++;
        }
    }
    
    // -order option
    // example: -order=id.ASC
    if(orderby) {
        char *order;
        
        strtok_r(orderby,".",&order);
        
        if(!order) {
            order = "";
        }
        
        xbuf_xcat(&query," ORDER BY %s %s", orderby, order);
    }
    
    char *endptr;
    
    // -limit option
    // example: -limit=1
    if(limit) {
        x_limit = strtoul(limit, &endptr, 10);
        if(endptr == limit) x_limit = 0;
        xbuf_xcat(&query," LIMIT %u", x_limit); 
    }
    // -offset option
    // example: -offset=7
    if(offset) {
        x_offset = strtoul(offset, &endptr, 10);
        if(endptr == offset) x_offset = 0;
        xbuf_xcat(&query," OFFSET %u", x_offset);
    }

    int ret = db_send(db,rep,query.ptr);

    xbuf_free(&query);

    if(ret) return ret;
   
    // add offset & limit headers
    xbuf_t headers;
    xbuf_init(&headers); 
    xbuf_xcat(&headers,"X-offset: %u\r\n", x_offset);
    xbuf_xcat(&headers,"X-limit: %u\r\n", x_limit);
    http_header(HEAD_ADD, headers.ptr, headers.len, argv);
    xbuf_free(&headers);  
    
    return 0;
}

int post_query(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t * db) {
    /*
      {
        type: 'sql',
        query : 'SELECT * FROM mytable WHERE userid = :userid',
        params : {
            'userid' : 400
        },
        gucs : {
            'work_mem' : '1M',
            'statement_timeout' : 5000,
            'search_path' : 'public,myschema'
        }
      }
    */

    // parse POST entity into json
    char *entity = (char*)get_env(argv, REQ_ENTITY);
    
    jsn_t *json = jsn_frtext(entity,0);

    if(!json) {
        return db_error(rep,422,"failed to parse json");
    }
    
    jsn_t *type = jsn_byname(json,"type",1);
    
    if(!type || type->type != jsn_STRING || strcmp(type->string,"sql") != 0) {
        jsn_free(json);
        
        return db_error(rep,400,"query [type] must be <sql>");
    }
    
    jsn_t *query = jsn_byname(json,"query",1);

    if(!query || query->type != jsn_STRING) {
        jsn_free(json);
        
        return db_error(rep,400,"[query] attribute must be <string>");
    }

    // todo: handle params
    
    // todo: handle gucs
    
    int ret = db_send(db,rep,query->string);

    jsn_free(json);
    
    return ret;
}


int main(int argc, char **argv){
    EndpointEntry endpoints[] = {
        {HTTP_GET, 0, get_query, 0},
        {HTTP_POST, 0, post_query, 0},
    };

    return exec_endpoint(argc,argv,endpoints,ArrayCount(endpoints));
}

