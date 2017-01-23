/* 
 * Copyright (c) 2015 Kaerus Software AB, all rights reserved.
 * Author Anders Elo <anders @ kaerus com>.
 *
 * Licensed under Apache 2.0 Software License terms, (the "License");
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma debug

#define DEBUG

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <errno.h>
#include <assert.h>
#include <event2/event.h>

#include "gwan.h"
#include "postgresql/libpq-fe.h"
#include "postrest/db.h"

#include "utils.c"


void cb(int sock, short what, void *arg)
{
  db_wait_t *result = arg;
  result->val = what;
  event_base_loopbreak(result->base);
}

int db_wait(void *conn, int flags, struct timeval *timeout, short *evalue)
{
  int fd = PQsocket((PGconn *)conn);

  struct event *ev;
  struct event_base *base = event_base_new();
  db_wait_t result;

  int ev_flags = flags|(timeout ? EV_TIMEOUT : 0);

  result.base = base;

  ev = event_new(base, fd, ev_flags, cb, &result);

  event_add(ev, timeout);

  event_base_dispatch(base);

  if(evalue) *evalue = result.val;

  return result.val & flags;
}


int db_error(xbuf_t *reply, int code, char *message){
  int status = code < 600 ? code : HTTP_400_BAD_REQUEST;
  code = status != code ? code : 0;

  if(message){
    xbuf_xcat(reply,
              "{\"status\":%d,\"error\":\"%s\",\"message\":\"%s\",\"code\":%d}",
              status, unquote(http_status(status)), unquote(message), code);
  } else {
    xbuf_xcat(reply,
              "{\"status\":%d,\"error\":\"%s\",\"message\":\"%m\",\"code\":%d}",
              status, unquote(http_status(status)), errno, errno);
  }
  return status;
}

int db_send(db_t *db, xbuf_t *rep, char *str){
  int count = 4;
  short evalue = 0;

  struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};

  debug_printf("--db_send\n");
  while(PQisBusy(db->conn) && count){
    count--;
    debug_printf("is busy\n");
    if(!db_wait(db->conn,EV_WRITE,&timeout, &evalue)){
      debug_printf("EV_WRITE result: %d\n", evalue);
	    return db_error(rep, HTTP_500_INTERNAL_SERVER_ERROR, 0);
    }
  }

  int ret = PQsendQuery(db->conn,str);

  PQsetSingleRowMode(db->conn);

  if(!ret) {
    return db_error(rep, HTTP_500_INTERNAL_SERVER_ERROR, PQerrorMessage(db->conn));
  }

  int flush = PQflush(db->conn);

  while(flush && count) {
    debug_printf("flushing %d\n",count);
    count--;

    if(!db_wait(db->conn,EV_WRITE,&timeout, &evalue)){
      debug_printf("EV_WRITE result: %d", evalue);
	    return db_error(rep, HTTP_500_INTERNAL_SERVER_ERROR, 0);
    }

    flush = PQflush(db->conn);
  }

  if(count <= 0){
    return db_error(rep, HTTP_429_TOO_MANY_REQUESTS, "failed to write query");
  }

  debug_printf("sent request\n");
  return 0;
}

int db_response(db_t *db, xbuf_t *rep, char **argv){
  u32 tuples, columns, items = 0, status = 0;
  char *fname = 0;
  short evalue = 0;

  struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};

  debug_printf("db_response\n");


  if(!db_wait(db->conn,EV_READ,&timeout,&evalue)){
    printf("consume error: %d\n", evalue);
    return db_error(rep, HTTP_500_INTERNAL_SERVER_ERROR, 0);
  } else {
    PQconsumeInput(db->conn);
  }

  PGresult *result;
  int single_row_mode = 0;
  // read until we get a null result
  // so that we consume the whole reply
  while((result = PQgetResult(db->conn))){
    debug_printf("PQresult\n");
  check_result:

    if(PQisBusy(db->conn)) {
      debug_printf("waiting to consume\n");
	    if(!db_wait(db->conn,EV_READ,&timeout,&evalue)){
        printf("consume error: %d\n", evalue);
        return db_error(rep, HTTP_500_INTERNAL_SERVER_ERROR, 0);
      } else {
        PQconsumeInput(db->conn);
      }
	    goto check_result;
    }

    status = PQresultStatus(result);

    if (status == PGRES_COMMAND_OK) continue; // no data

    if (status != PGRES_TUPLES_OK && status != PGRES_SINGLE_TUPLE) {
	    PQclear(result);

	    // empties command buffer
	    result = PQgetResult(db->conn);

	    if(result) goto check_result;

	    // clear reply buffer
	    xbuf_reset(rep);

	    return db_error(rep, HTTP_400_BAD_REQUEST, PQerrorMessage(db->conn));
    }

    // todo: fix so that we can handle partial results
    // now we expect only a single pass of tuples.

    columns = PQnfields(result);
    tuples = PQntuples(result);

    if(status == PGRES_SINGLE_TUPLE) {
	    single_row_mode = 1;
	    tuples = 1;
    }

    if(tuples && columns) {
	    if(!items) {
        xbuf_xcat(rep,"{\"code\":%d,\"result\":[ ",status);
	    }

	    for (int row = 0; row < tuples; row++) {
        bool inObject = false;
        char *json;
        for(int col = 0; col < columns; col++) {

          // field name
          fname = PQfname(result,col);

          json = strstr(fname,"json");

          if(!json) {
            if(!inObject) {
              xbuf_xcat(rep,"{");
              inObject = true;
            }
            xbuf_xcat(rep,"\"%s\":", fname);

            // field format, 0 = text, 1 = binary
            if(PQfformat(result, col) == 0){
              // text data
              xbuf_xcat(rep,"\"%s\"",PQgetvalue(result, row, col));
            }
            else if(PQgetisnull(result, row, col)){
              xbuf_xcat(rep,"null");
            } else {
              // binary data in network order
              xbuf_t pqdata;
              xbuf_init(&pqdata);

              pqdata.ptr = PQgetvalue(result, row, col);
              pqdata.len = PQgetlength(result, row, col);
              // base64 encode
              xbuf_xcat(rep,"\"%12B\"",&pqdata);
            }
          } else {
            if(columns > 1) xbuf_xcat(rep,"\"%s\":",(strlen(json) > 5 ? json+5 : "json"));
            xbuf_xcat(rep,PQgetvalue(result, row, col));
          }
          if(col < columns - 1) xbuf_xcat(rep,",");
          else if(inObject) xbuf_xcat(rep,"}");
        }
        if(single_row_mode || row < tuples - 1) xbuf_xcat(rep,",");
	    }
	    items+= tuples;
    } else {
	    if(status == PGRES_TUPLES_OK && single_row_mode){
        rep->ptr[rep->len-1] = ' ';
	    }
    }
    PQclear(result);
  }

  if(items) xbuf_xcat(rep,"],\"items\":%d}",items);
  else xbuf_xcat(rep,"{\"code\":%d,\"items\":0}", status);

  char hdr[64];
  s_snprintf(hdr,63,"X-items: %u\r\n", items);
  http_header(HEAD_ADD, hdr, strlen(hdr), argv);

  return 200;
}

void db_free(char **argv, db_t *db){
  session_t *sessions = *((session_t**) get_env(argv, US_SERVER_DATA));

  assert(sessions);

  debug_printf("remove session(%s)\n", db->skey);

  kv_del(sessions, db->skey, strlen(db->skey));
  PQfinish(db->conn);
  free(db->schema);
  free(db->skey);
}


int db_close(char **argv){
  char *ssid = *((char **) get_env(argv,US_REQUEST_DATA));

  if(!ssid) return 0;

  session_t *sessions = *((session_t**) get_env(argv, US_SERVER_DATA));

  db_t *db = (db_t *) kv_get(sessions,ssid,strlen(ssid));

  if(!db) return 0;

  debug_printf("terminating session: %s\n", db->skey);

  db_free(argv,db);

  return 1;
}


int db_isConnected(void *conn){

  int s = PQsocket((PGconn *)conn);
  int err;
  socklen_t len = sizeof(int);

  fd_set rds;
  fd_set wrs;
  FD_ZERO(&rds);
  FD_SET(s, &rds);
  wrs = rds;

  errno = 0; // reset errno

  if(!FD_ISSET(s, &rds) && !FD_ISSET(s, &wrs) ){
    debug_printf("FD_IS NOT SET\n");
    return 0;
  }

  if(getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len) < 0){
    debug_printf("get sockopt error:%s\n", strerror(errno));
    return 0;
  }

  errno = err;

  return err == 0;
}


typedef struct {
  kv_item *item;
} ctx_t;


int check_session(const kv_item *item, const void *ctx){
  db_t *db;

  if(item && (db = (db_t *)item->val)) {
    u64 now = getms();
    s32 timeout = db->timeout - now;
    if(timeout < 0){
      kv_t *sessions = (kv_t *)ctx;
      debug_printf("deleting key %s\n", item->key);
      kv_del(sessions, item->key, 0);
      PQfinish(db->conn);
      free(db->schema);
      free(db->skey);
    }
  }

  return 1;
}

void db_timer_cb(int fd, short event, void *arg){
  server_t *server = (server_t *) arg;
  kv_do(server->sessions, 0, 0, check_session, (void *)server->sessions);
}

db_t *db_session(char **argv) {

  server_t *server = *((server_t**) get_env(argv, US_SERVER_DATA));

  if(!server) return 0;

  struct event_base *base = (struct event_base *) kv_get(server->config,"evbase",6);

  if(base) {
    debug_printf("base pointer %p\n",base);
    event_base_loop(base, EVLOOP_NONBLOCK);
  }

  char **skey = (char **) get_env(argv,US_REQUEST_DATA);

  if(!*skey) {
    int auth_type = get_env(argv, AUTH_TYPE);

    if(auth_type == AUTH_BASIC) {
	    http_t *http = (http_t *) get_env(argv, HTTP_HEADERS);

	    char session_key[128] = {};

	    s_snprintf(session_key,sizeof(session_key)-1,"%s:%s", http->h_auth_user, http->h_auth_pwd);

      // note: gc_malloc fails here
	    *skey = strdup(session_key);

    } else return 0;
  }

  db_t *db = (db_t *) kv_get(server->sessions,*skey,strlen(*skey));

  // update timestamp
  if(db) {
    db->timestamp = getms();
    db->timeout = db->timestamp + DB_SESSION_TIMEOUT;
  }

  /* todo: fixme
     if(db && !db_isConnected(db->conn)) {
     debug_printf("socket disconnected!\n");
     db_free(argv,db);

     return 0;
     }
  */

  return db;
}

