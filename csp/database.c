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

#include "gwan.h"
#include "postgresql/libpq-fe.h"
#include "postrest/db.h"

int get_list(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    int ret = db_send(db,rep,
        "SELECT datname AS database FROM pg_database "
        "WHERE(datistemplate = false);");
    
    return ret;
}

int get_tables(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    xbuf_t query;
    xbuf_init(&query);
    
    xbuf_xcat(&query,
        "SELECT row_to_json(t) FROM pg_catalog.pg_tables AS t "
        "WHERE schemaname != 'pg_catalog' "
        "AND schemaname != 'information_schema';");
    
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);
    
    return ret;
}

int get_stats(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    int ret = db_send(db,rep,
        "SELECT row_to_json(t) FROM pg_stat_database AS t;");
    
    return ret;
}


int get_users(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    int ret = db_send(db,rep,
        "SELECT to_json(u) FROM pg_catalog.pg_user AS u;");
    
    return ret;
}

int get_grants(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    int ret = db_send(db,rep,
        "SELECT to_json(g) FROM information_schema.role_table_grants AS g;");
    
    return ret;
}

int get_active(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    char *state = argv[0];
    char *name = argv[1];
    char asdb[256] = {};
    
    debug_printf("%s %s\n",state, name);
    
    if(!strword(name)) {
        name = 0;
    } else {
        s_snprintf(asdb,sizeof(asdb)-1,"AND datname = '%s'", name);
    }
    xbuf_t query;
    xbuf_init(&query);
    
    xbuf_xcat(&query,
        "SELECT json_build_object('id',a.datid, 'database', a.datname,"
        "'pid',a.pid, 'usesysid', a.usesysid, 'usename',a.usename,"
        "'waiting',a.waiting, 'xact_start', a.xact_start,"
        "'query_start',a.query_start,'state_change',a.state_change) "
        "FROM pg_stat_activity AS a WHERE(state = '%s' %s);", state, name ? asdb : "");
    
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);
    
    return ret;
}

int get_version(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {

    int server_version = PQserverVersion(db->conn);
    
    if(!server_version) {
        return db_error(rep,500,"connection error");
    }
    
    Reply("{\"server\":%d,\"adapter\":%d}",server_version, ADAPTER_VERSION);
    
    return 200;
}

int vacuum_database(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    /* PUT _database/vacuum
     * {
     *   options: <string>,
     *   table: <string>,
     *   columns: <string>
     * }
     *
     */
    char *entity = (char*)get_env(argv, REQ_ENTITY);
    
    xbuf_t options;
    xbuf_init(&options);
    xbuf_t columns;
    xbuf_init(&columns);
    char *tablename = 0;
     
    if(entity) {
        jsn_t *json = jsn_frtext(entity,0);

        if(!json){
            jsn_free(json);
            
            return db_error(rep,400,"failed to parse json");
        }
        
        jsn_t *op = jsn_byname(json,"options",1);
    
        if(op && op->type != jsn_NODE){
            jsn_free(json);
            
            return db_error(rep,400,"[options] must be an <object>");
        }
        
        if(op){
            char *oplist[4] = {};
            int o = 0;
            
            jsn_t *full = jsn_byname(op,"full",1);
            jsn_t *freeze = jsn_byname(op,"freeze",1);
            jsn_t *verbose = jsn_byname(op,"verbose",1);
            jsn_t *analyze = jsn_byname(op,"analyze",1);
            
            if(full && full->type == jsn_TRUE) oplist[o++] = "FULL";
            if(freeze && freeze->type == jsn_TRUE) oplist[o++] = "FREEZE";
            if(verbose && verbose->type == jsn_TRUE) oplist[o++] = "VERBOSE";
            if(analyze && analyze->type == jsn_TRUE) oplist[o++] = "ANALYZE";
            
            if(o) strjoin(&options,oplist,o,'(',')',',');
        }
        
        jsn_t *tab = jsn_byname(json,"table",1);
    
        if(tab && tab->type != jsn_STRING && !strword(tab->string)){
            jsn_free(json);
            
            return db_error(rep,400,"[table] must be a word <string>");
        }
        
        if(tab) tablename = tab->string;
        
        jsn_t *col = jsn_byname(json,"columns",1);
    
        if(col && col->type != jsn_ARRAY){
            jsn_free(json);
            
            return db_error(rep,400,"[columns] must be an <array>");
        }
        
        if(col) {
            char *colist[16] = {};
            int ci = 0;
            
            jsn_t *item;
            
            while((item = jsn_byindex(col, ci))){
                if(item->type != jsn_STRING && !strword(item->string)){
                    jsn_free(json);
                    
                    return db_error(rep,400,"[columns] <array> items must be a word <string>");
                }
                
                colist[ci++] = item->string;
            }
            
            if(ci) strjoin(&columns,colist,ci,'(',')',',');
        }
        
        jsn_free(json);
    }
    
    // VACUUM [ ( { FULL | FREEZE | VERBOSE | ANALYZE } [, ...] ) ] [ table_name [ (column_name [, ...] ) ] ]

    xbuf_t query;
    xbuf_init(&query);
    xbuf_xcat(&query,
        "VACUUM %s %s %s;"
        ,options.ptr ? options.ptr : ""
        ,tablename ? tablename : ""
        ,tablename ? columns.ptr : "");
        
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);
    xbuf_free(&options);
    xbuf_free(&columns);
    
    return ret;
}

int create_database(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    char *name = argv[0];
    
    if(!strword(name)) {
        return db_error(rep,422,"[database] name must be a word <string>");
    }
    
    char *entity = (char*)get_env(argv, REQ_ENTITY);
    
    if(entity){
        jsn_t *json = jsn_frtext(entity,0);

        // TODO: use options from json entity
        /* CREATE DATABASE db_name
            OWNER =  role_name
            TEMPLATE = template
            ENCODING = encoding
            LC_COLLATE = collate
            LC_CTYPE = ctype
            TABLESPACE = tablespace_name
            CONNECTION LIMIT = max_concurrent_connection
        */ 
        
        if(!json){            
            return db_error(rep,400,"failed to parse json");
        }
        
        jsn_free(json);
    }

    
    xbuf_t query;
    xbuf_init(&query);
    
    xbuf_xcat(&query,
	      "CREATE DATABASE %s "
	      "ENCODING = 'UTF8' "
	      "CONNECTION LIMIT = 8 "
	      "TEMPLATE = template_postrest;"
        , name);
    
    int ret = db_send(db,rep,query.ptr);
    
    debug_printf("create database %s\n", name);

    xbuf_free(&query);
    
    return ret;
}

int shutdown_database(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    char *name = argv[1];
    
    if(!strword(name)) {
        return db_error(rep,422,"[database] name must be a word <string>");
    }
    
    
    xbuf_t query;
    xbuf_init(&query);
    debug_printf("shutdown database %s\n", name);
    
    xbuf_xcat(&query, 
        "ALTER DATABASE %s CONNECTION LIMIT -1;"
        "SELECT pg_terminate_backend(pid),json_build_object('name',datname, 'connection_limit','-1') FROM pg_stat_activity WHERE datname = '%s';"
        , name, name);
            
    int ret = db_send(db,rep,query.ptr);

    xbuf_free(&query);
    
    return ret;
} 

int delete_database(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    char *name = argv[0];
    
    if(!strword(name)) {
        return db_error(rep,422,"[database] name must be a word <string>");
    }
    
    xbuf_t query;
    xbuf_init(&query);
    debug_printf("delete database %s\n", name);
    
    xbuf_xcat(&query,"DROP DATABASE %s;", name);
            
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);
    
    return ret;
}

int main(int argc, char **argv){
    EndpointEntry endpoints[] = {
        {HTTP_GET, "list", get_list, 0},
        {HTTP_GET, "tables", get_tables, 0},
        {HTTP_GET, "stats", get_stats, 0}, 
        {HTTP_GET, "users", get_users, 0},
        {HTTP_GET, "grants", get_grants, 0},
        {HTTP_GET, "active", get_active, 0},
        {HTTP_GET, "version", get_version, 0},
        {HTTP_PUT, "vacuum", vacuum_database, 0},
        {HTTP_PUT, "shutdown", shutdown_database, 0},
        {HTTP_POST, 0, create_database, 0},
        {HTTP_DELETE, 0, delete_database, 0},
    };

    return exec_endpoint(argc,argv,endpoints,ArrayCount(endpoints));
}

