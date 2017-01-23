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

int get_graph(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    return HTTP_501_NOT_IMPLEMENTED;
}

/* GET _graph/node/:graph/:label/:[to|from]
 * Get nodes with an edge label
 */
int get_node(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    char *graph = argv[1];
    
    if(!strword(graph)) {
        return db_error(rep,422,"[graph] name must be a word <string>");
    }

    char *label = 0;

    if(argc > 2){
        label = argv[2];

	if(!isString(label)){
	    return db_error(rep,422,"[label] must be a <string>");
	}
    } else {
        return db_error(rep,422,"missing [label] parameter");
    }
   
    char *direction = 0;

    if(argc > 3){
        label = argv[3];

	if(!strword(direction)){
	    return db_error(rep,422,"[direction] must be 'to' or 'from' <string>");
	}
    }
  
    debug_printf("get label(%s)\n",label);
    
    xbuf_t query;
    xbuf_init(&query);

    xbuf_xcat(&query,
              "SELECT id, xmin AS rev, label, document AS json_doc "
              "FROM %s.%s.%s_graph_edges AS E "
              "INNER JOIN %.%.%._graph_collection AS N "
              ,db->name, db->schema, graph
              ,db->name, db->schema, graph);

    if(!direction || strcmp(direction,"from") != 0){
        xbuf_xcat(&query,
                  "ON N.id = E.b WHERE E.label = '%s';" // edge to->node
                  ,label);
    } else {
        xbuf_xcat(&query,
                  "ON N.id = E.a WHERE E.label = '%s';" // edge from->node
                  ,label);
    }
    
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);
    
    return ret;
}

/* GET _graph/adjacent/:graph/:uuid
 *
 */
int get_adjacent(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {

// get adjacent nodes
// select id, name, prop, label from mygraph_graph_edges as E 
// left join mygraph_graph_nodes as N on N.id = E.a 
// where E.a = '9c790689-1484-4191-b876-4d2fdcb22f3f'; 
    char *graph = 0;
    
    if(argc > 1){
        graph = argv[1];
    }
     
    if(!strword(graph)) {
        return db_error(rep,422,"[graph] name must be a word <string>");
    }
    
    char *uuid = 0;

    if(argc > 2){
	if(strncmp(argv[2],"null",4) != 0){
	    uuid = argv[2];
	}
    }
    
    if(uuid && !isUUID(uuid)) {
        return db_error(rep,422,"node [id] must be an uuid <string> or null");
    }

    char *label = 0;

    if(argc > 3){
        label = argv[3];

	if(!isString(label)){
	    return db_error(rep,422,"[label] must be a <string>");
	}
    }
    
    xbuf_t query;
    xbuf_init(&query);

    // todo: consider creating option not including json_doc 
    xbuf_xcat(&query,
	      "SELECT id, label, name, document AS json_doc "
	      "FROM %s.%s.%s_graph_edges AS E "
	      "INNER JOIN %s.%s.%s_graph_collection AS N "
	      ,db->name, db->schema, graph
	      ,db->name, db->schema, graph);
    
    if(uuid){
	xbuf_xcat(&query,"ON N.id = E.b WHERE E.a = '%s'", uuid);
    } else xbuf_xcat(&query,"ON N.id = E.b WHERE E.a IS NULL");
    
    if(label){
	xbuf_xcat(&query," AND E.label = '%s';", label);
    } else {
	xbuf_xcat(&query,";");
    }
    
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);
    
    return ret;

}

/* GET _graph/traverse/:graph/:from/:to/:label
 *
 */
int traverse(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {


    char *graph = 0;
    
    if(argc > 1){
        graph = argv[1];
    }
     
    if(!strword(graph)) {
        return db_error(rep,422,"[graph] name must be a word <string>");
    }
    
    char *from = 0;

    if(argc > 2){
	if(strncmp(argv[2],"null",4) != 0){
	    from = argv[2];
	}
    }
    
    if(from && !isUUID(from)) {
        return db_error(rep,422,"[from] must be an uuid <string> or null");
    }

    char *to = 0;

    if(argc > 3){
	if(strncmp(argv[3],"null",4) != 0){
	    to = argv[3];
	}
    }
    
    if(to && !isUUID(to)) {
        return db_error(rep,422,"[to] must be an uuid <string> or null");
    }

    char *label = 0;

    if(argc > 4){
        label = argv[4];
    }

    if(label && !strword(label)){
	return db_error(rep,422,"[label] must be an word <string>");
    }
    
    xbuf_t query;
    xbuf_init(&query);
    
    char tableprefix[256] = {};
    s_snprintf(tableprefix,sizeof(tableprefix)-1,"%s.%s.%s", db->name, db->schema, graph);

    /*
      WITH RECURSIVE traverse_graph(a,b,label,distance,ids) AS (
      SELECT e.a, e.b, e.label,1 AS distance, ARRAY[e.b] AS id FROM project_graph_edges e
      WHERE e.a IS NULL
      UNION 
      SELECT tc.a, e.b, e.label, tc.distance + 1, ids || e.b
      FROM project_graph_edges as e
      ,traverse_graph AS tg 
      WHERE e.a = tg.b)
      select id, name from project_graph_collection as node 
      where node.id = ANY((SELECT ids FROM traverse_graph where b = '5efb293d-c9b3-47e9-b1cd-3aa5ce46d47d')::uuid[])


      another variant
      WITH RECURSIVE traverse_graph(a,b,label,distance,nodes) AS (
      SELECT e.a, e.b, e.label,1 AS distance, ARRAY[e.b] AS id FROM project_graph_edges e
      WHERE e.a IS NULL
      UNION 
      SELECT tc.a, e.b, e.label, tc.distance + 1, nodes || e.b
      FROM project_graph_edges as e
      ,traverse_graph AS tc 
      WHERE e.a = tc.b)
      select id, name from project_graph_collection as node
      JOIN unnest((SELECT nodes FROM traverse_graph where b = '0d0df90f-d74e-4951-899a-98d543482a6a')::uuid[]) as x(id) using (id)
      

    */
    
    xbuf_xcat(&query,
	      "WITH RECURSIVE search_graph(id, name, depth, path, nodes,cycle)"
	      "AS ("
	      "SELECT n.id, n.name, 1, ARRAY[n.name], ARRAY[n.id],false "
	      "FROM %s_graph_edges AS e "
	      "INNER JOIN %s_graph_collection AS n ON n.id = e.b "
	      ,tableprefix,tableprefix);

    if(from){
	xbuf_xcat(&query,"WHERE e.a = '%s' ",from);
    } else {
	xbuf_xcat(&query,"WHERE e.a IS NULL ");
    }

    if(label){
	xbuf_xcat(&query,"AND e.label = '%s' ", label);
    }
    
    xbuf_xcat(&query,
	      "UNION "
	      "SELECT n.id, n.name, sg.depth + 1, path || n.name, nodes || n.id, n.name = ANY(path) "
	      "FROM %s_graph_edges AS e "
	      "INNER JOIN %s_graph_collection AS n ON n.id = e.b, "
	      "search_graph sg WHERE e.a = sg.id "
	      ,tableprefix,tableprefix);

    if(label){
	xbuf_xcat(&query,"AND e.label = '%s' ", label);
    }
    
    xbuf_xcat(&query,"AND NOT cycle) ");
    
    xbuf_xcat(&query,
	      "SELECT id,name,depth,"
	      "array_to_json(path) AS json_path,"
	      "array_to_json(nodes) AS json_nodes "
	      "FROM search_graph ");

    if(to){
	xbuf_xcat(&query,"WHERE id = '%s';", to);
    } else {
	xbuf_xcat(&query,"ORDER BY depth;");
    }
    
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);
    
    return ret;

}

int update_graph(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    return HTTP_501_NOT_IMPLEMENTED;
}



int delete_graph(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {

    char *drop_method = "RESTRICT";
    char *name = argv[1];
    char *cascade = argv[2] ? argv[2] : 0;
    
    if(!name || !strword(name)){
        return db_error(rep, 400,"[graph] name must be a word <string>");
    }
    
    if(cascade && strcmp(cascade,"cascade") == 0) drop_method = "CASCADE";
     
    debug_printf("name(%s) drop_method(%s)", name, drop_method);
     
    char table[256] = {};
    
    s_snprintf(table,sizeof(table)-1,"%s.%s.%s",db->name,db->schema,name);
    
    xbuf_t query;
    xbuf_init(&query);
    
    xbuf_xcat(&query,
              "DROP TABLE %s_graph_edges, %s_graph_collection %s;"
              ,table, table, drop_method);

    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);
    
    return ret;
}

int create_graph(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    /* create graph
       {
       "name": "<name>"
       }
    */
    
    char *entity = (char*)get_env(argv, REQ_ENTITY);
    
    jsn_t *json = jsn_frtext(entity,0);

    if(!json) {
        return db_error(rep, 400,"failed to parse json");
    }

    jsn_t *name = jsn_byname(json,"name",1);
    
    if(!name || name->type != jsn_STRING || !strword(name->string)){
        jsn_free(json);
        
        return db_error(rep, 400,"[graph] name must be a word <string>");
    }
    
    char graph[256] = {};
    s_snprintf(graph,sizeof(graph)-1,"%s.%s.%s", db->name, db->schema, name->string);
     
    xbuf_t query;
    xbuf_init(&query);
    
    // graph nodes
    // note: same structure as document so it could be merged
    xbuf_xcat(&query,
	      "CREATE TABLE %s_graph_collection ("
	      "id uuid PRIMARY KEY DEFAULT gen_random_uuid(),"
	      "name text,"
	      "document jsonb );"
	      , graph);
    
    // index json document    
    xbuf_xcat(&query,
	      "CREATE INDEX document_%s_graph_idx ON %s_graph_collection "
	      "USING GIN (document jsonb_path_ops);"
	      , name->string, graph);
    
    // index name if present
    xbuf_xcat(&query,
	      "CREATE INDEX name_%s_graph_idx ON %s_graph_collection(name) "
	      "WHERE name IS NOT NULL;"
    	      , name->string, graph);
    
    // graph edges table
    xbuf_xcat(&query,    
	      "CREATE TABLE %s_graph_edges ("
	      "a uuid REFERENCES %s_graph_collection(id) "
	      "ON UPDATE CASCADE ON DELETE CASCADE,"
	      "b uuid REFERENCES %s_graph_collection(id) "
	      "ON UPDATE CASCADE ON DELETE CASCADE,"
	      "label text,"
	      "UNIQUE (a,b),"
	      "CONSTRAINT %s_graph_prevent_self CHECK (a <> b) );"
	      , graph, graph, graph, name->string);
    // "PRIMARY KEY (a,b),"
    xbuf_xcat(&query,
              "CREATE INDEX a_%s_idx ON %s_graph_edges(a);"
              , name->string, graph);
          
    xbuf_xcat(&query,
              "CREATE INDEX b_%s_idx ON %s_graph_edges(b);"
              , name->string, graph);
    
    jsn_free(json);
    
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);
    
    return ret;
}

/* POST _graph/edge/:graph
 * entity: json { <object> }
 */
int create_edge(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    char *graph = 0;

    if(argc > 1){
        graph = argv[1];
    }
    
    if(!strword(graph)) {
        return db_error(rep,422,"[graph] name must be a word <string>");
    }
    
    char *entity = (char*)get_env(argv, REQ_ENTITY);
    
    jsn_t *json = jsn_frtext(entity,0);

    if(!json) {
        return db_error(rep, 400,"failed to parse json");
    }

    jsn_t *a = jsn_byname(json,"from",1);

    if(!a) {
        jsn_free(json);
        return db_error(rep,400,"[from] edge node is null");
    } else if(a->type == jsn_NULL){
	a = 0;
    } else if(a->type == jsn_STRING && !isUUID(a->string)){
        jsn_free(json);
	return db_error(rep,400,"[from] edge node must be uuid <string>");
    } 

    jsn_t *b = jsn_byname(json,"to",1);

    if(!b) {
        jsn_free(json);
        return db_error(rep,400,"[to] edge node is null");
    } else if(b->type == jsn_NULL){
	b = 0;
    } else if(b->type == jsn_STRING && !isUUID(b->string)){
        jsn_free(json);
	return db_error(rep,400,"[to] edge node must be uuid <string>");
    }

    jsn_t *l = jsn_byname(json,"label",1);
    
    if(l && (l->type != jsn_STRING || !isString(l->string))){
        jsn_free(json);
	return db_error(rep,400,"[label] name must be a <string>");
    }


    char *label = l ? l->string : "";
    
    xbuf_t query;
    xbuf_init(&query);

    if(a->type == jsn_ARRAY) {
        int i = 0;
        jsn_t *ia, *ib;
            
        while((ia = jsn_byindex(a,i))){

            if(b->type == jsn_ARRAY) {
                ib = jsn_byindex(b,i);
            } else {
                ib = b;
            }

            if(ib){
                xbuf_xcat(&query,
                          "INSERT INTO %s.%s.%s_graph_edges VALUES('%s','%s','%s');"	      
                          ,db->name, db->schema, graph, ia->string, ib->string, label);
            } else {
                xbuf_xcat(&query,
                          "INSERT INTO %s.%s.%s_graph_edges VALUES('%s',NULL,'%s');"	      
                          ,db->name, db->schema, graph, ia->string, label);
                
            }
            i++;
        }
        
    } else if(b->type == jsn_ARRAY) {
        int i = 0;
        jsn_t *ib;
            
        while((ib = jsn_byindex(b,i))){
            
            if(a){
                xbuf_xcat(&query,
                          "INSERT INTO %s.%s.%s_graph_edges VALUES('%s','%s','%s');"	      
                          ,db->name, db->schema, graph, a->string, ib->string, label);
            } else {
                xbuf_xcat(&query,
                          "INSERT INTO %s.%s.%s_graph_edges VALUES(NULL,'%s','%s');"	      
                          ,db->name, db->schema, graph, ib->string, label);
               
            }
            i++;
        }
        
    } else {
        
        if(a) {
            xbuf_xcat(&query,
                      "INSERT INTO %s.%s.%s_graph_edges VALUES('%s','%s','%s');"	      
                      ,db->name, db->schema, graph, a->string, b->string, label);
        } else {
            xbuf_xcat(&query,
                      "INSERT INTO %s.%s.%s_graph_edges VALUES(NULL,'%s','%s');"	      
                      ,db->name, db->schema, graph, b->string, label);
        }
    }
    
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);  
    jsn_free(json);

    return ret;
}

/* DELETE _graph/edge/:graph
 * entity: json { <object> }
 */
int delete_edge(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    char *graph = 0;

    if(argc > 1){
        graph = argv[1];
    }
    
    if(!strword(graph)) {
        return db_error(rep,422,"[graph] name must be a word <string>");
    }
    
    char *entity = (char*)get_env(argv, REQ_ENTITY);
    
    jsn_t *json = jsn_frtext(entity,0);

    if(!json) {
        return db_error(rep, 400,"failed to parse json");
    }

    jsn_t *a = jsn_byname(json,"from",1);

    if(!a) {
        jsn_free(json);
        return db_error(rep,400,"[from] edge node is null");
    } else if(a->type == jsn_NULL){
	a = 0;
    } else if(a->type == jsn_STRING && !isUUID(a->string)){
        jsn_free(json);
	return db_error(rep,400,"[from] edge node must be uuid <string>");
    } 

    jsn_t *b = jsn_byname(json,"to",1);

    if(!b) {
        jsn_free(json);
        return db_error(rep,400,"[to] edge node is null");
    } else if(b->type == jsn_NULL){
	b = 0;
    } else if(b->type == jsn_STRING && !isUUID(b->string)){
        jsn_free(json);
	return db_error(rep,400,"[to] edge node must be uuid <string>");
    }

    jsn_t *l = jsn_byname(json,"label",1);
    
    if(l && (l->type != jsn_STRING || !isString(l->string))){
        jsn_free(json);
	return db_error(rep,400,"[label] name must be a <string>");
    }


    char *label = l ? l->string : "%";
    
    xbuf_t query;
    xbuf_init(&query);

    if(a->type == jsn_ARRAY) {
        int i = 0;
        jsn_t *ia, *ib;
            
        while((ia = jsn_byindex(a,i))){

            if(b->type == jsn_ARRAY) {
                ib = jsn_byindex(b,i);
            } else {
                ib = b;
            }

            if(ib){
                xbuf_xcat(&query,
                          "DELETE FROM %s.%s.%s_graph_edges WHERE a = '%s' AND b = '%s' AND label = '%s';"	      
                          ,db->name, db->schema, graph, ia->string, ib->string, label);
            } else {
                xbuf_xcat(&query,
                          "DELETE FROM %s.%s.%s_graph_edges WHERE a = '%s' AND label = '%s';"	      
                          ,db->name, db->schema, graph, ia->string, label);
                
            }
            i++;
        }
        
    } else if(b->type == jsn_ARRAY) {
        int i = 0;
        jsn_t *ib;
            
        while((ib = jsn_byindex(b,i))){
            
            if(a){
                xbuf_xcat(&query,
                          "DELETE FROM %s.%s.%s_graph_edges WHERE a = '%s' AND b = '%s' AND label = '%s';"	      
                          ,db->name, db->schema, graph, a->string, ib->string, label);
            } else {
                xbuf_xcat(&query,
                          "DELETE FROM %s.%s.%s_graph_edges WHERE a = '%s' AND label = '%s';"	      
                          ,db->name, db->schema, graph, ib->string, label);
               
            }
            i++;
        }
        
    } else if(a || b) {
        
        if(a) {
            xbuf_xcat(&query,
                      "DELETE FROM %s.%s.%s_graph_edges WHERE a = '%s' AND b = '%s' AND label = '%s';"	      
                      ,db->name, db->schema, graph, a->string, b->string, label);
        } else {
            xbuf_xcat(&query,
                      "DELETE FROM %s.%s.%s_graph_edges WHERE a = '%s' AND label = '%s';"	      
                      ,db->name, db->schema, graph, b->string, label);
        }
    } else {
        xbuf_xcat(&query,
                  "DELETE FROM %s.%s.%s_graph_edges WHERE label = '%s';"	      
                  ,db->name, db->schema, graph, label);
        
    }
    
    int ret = db_send(db,rep,query.ptr);
    
    xbuf_free(&query);  
    jsn_free(json);

    return ret;

}

int main(int argc, char **argv){
    EndpointEntry endpoints[] = {
        {HTTP_GET, "adjacent", get_adjacent, 0},
        {HTTP_GET, "traverse", traverse, 0},
        {HTTP_GET, "node", get_node, 0}, 
        {HTTP_POST, "graph", create_graph, 0},
        {HTTP_POST, "edge", create_edge, 0},
        {HTTP_DELETE, "graph", delete_graph, 0},
        {HTTP_DELETE, "edge", delete_edge, 0},
    };

    return exec_endpoint(argc,argv,endpoints,ArrayCount(endpoints));
}


