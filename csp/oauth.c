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


typedef enum {
    OAUTH_HMAC = 0, // HMAC-SHA1
    OAUTH_RSA,      // RSA
    OAUTH_TEXT      // plain text
} OAuthSignature;

/*
 * Oauth Authorization steps
 *   [client <-> server]
 * 1. Authorization request ->
 * 2. <- Authorization code grant
 * 3. AccessToken request ->
 * 4. <- AccessToken grant
 * 5. RefreshToken ->
 *
 * 1) //<server_url>/oauth/authorize?response_type=code&client_id=<CLIENT_ID>&redirect_uri=<CALLBACK_URL>&scope=<SCOPE>
 * 2) //<client_url>/callback?code=<AUTHORIZATION_CODE>
 * 3) //<server_url>/oauth/token?client_id=<CLIENT_ID>&client_secret=<CLIENT_SECRET>&grant_type=authorization_code&code=<AUTHORIZATION_CODE>&redirect_uri=<CALLBACK_URL>
 * 4) //<client_url>/callback + json data
 * {"access_token":"ACCESS_TOKEN",
 *  "token_type":"bearer",
 *  "expires_in":2592000,
 *  "refresh_token":"REFRESH_TOKEN",
 *  "scope":"read",
 *  "uid":100101,
 *  "info":{"db":"database"}}
 * 5) //<server_url>/oauth/token?grant_type=refresh_token&client_id=<CLIENT_ID>&client_secret=<CLIENT_SECRET>&refresh_token=<REFRESH_TOKEN>
 */


// step 1
int get_authorize(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {

    debug_printf("Oauth step 1\n");

    for(int i = 0; i < argc; i++ ) {
        debug_printf("argv%d(%s)\n", i, argv[i]);
    }

    return HTTP_202_ACCEPTED;
}

// step 2
int get_token(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {
    debug_printf("Oauth step 2\n");
    debug_printf("argv1(%s) argv2(%s)\n", argv[0], argv[1]);

    xbuf_xcat(rep,"{\"token\":\"1234ABCDEF\"}");

    return HTTP_230_AUTHENTICATION_SUCCESSFUL;
}

struct hawk_artifacts {
    char *method; // request.method,
    char *host; // request.host,
    char *port; // request.port,
    char *resource; // request.url,
    char *ts; // attributes.ts,
    char *nonce; // attributes.nonce,
    char *hash; // attributes.hash,
    char *ext; // attributes.ext,
    char *app; // attributes.app,
    char *dlg; // attributes.dlg,
    char *mac; // attributes.mac,
    char *id; //attributes.id
};

char *hawk_parse_token(char *s, char **p){
    if(!p || !*p) return 0;

    char *token = *p;
    int len = strlen(s);
    bool unquote = false;

    if(len < 2) return 0;

    char *str = strstr(token,s);

    if(!str) return 0;

    token = str + len - 1;
    *token = 0;
    token++;

    if(*token == '"') {
        unquote = true;
        token++;
    }

    str = strchr(token,',');

    if(str){
        *p = str + 1;
    } else {
        str = token + strlen(token);
        *p = 0;
    }

    if(unquote){
        *(str-1) = 0;
    } else *str = 0;

    return token;
}

//Hawk id="dh37fgj492je", ts="1446479411", nonce="faPseY", ext="and welcome!", mac="yq9qffcrW4wVifTZ5vxsDYYEDzIBr+IdqKZQ5v7TQag="
int hawk_parse_artifacts(char *authorization, struct hawk_artifacts *artifacts){
    if(strncmp(authorization,"Hawk",4) != 0 ) return 0;

    if(strlen(authorization) < 5) return 1;

    char *p = authorization + 4;
    *p = 0;
    p++;

    artifacts->id = hawk_parse_token("id=",&p);
    artifacts->ts = hawk_parse_token("ts=",&p);
    artifacts->nonce = hawk_parse_token("nonce=",&p);
    artifacts->ext = hawk_parse_token("ext=",&p);
    artifacts->mac = hawk_parse_token("mac=",&p);
    //artifacts->app = hawk_parse_token("app=",&p);
    //artifacts->dlg = hawk_parse_token("dlg=",&p);
    //artifacts->hash = hawk_parse_token("hash=",&p);

    return 1;
}

int get_all(int argc, char **argv, xbuf_t *req, xbuf_t *rep, db_t *db) {

    for(int i = 0; i < argc; i++ ) {
        debug_printf("argv%d(%s)\n", i, argv[i]);
    }

    http_headers_t headers[24] = {};

    int count = get_headers(req,headers,24);

    debug_printf("Found %d headers\n", count);

    for(int i = 0; i < count; i++){
        debug_printf("%s:%s\n", headers[i].name, headers[i].value);
    }

    struct hawk_artifacts artifacts;

    char *szRequest = (char*)get_env(argv, REQUEST);

    int szLen = strlen(szRequest);
    debug_printf("szRequest %s\n", szRequest);
    for(int i = 0; i < szLen; i++) {
        if (szRequest[i] == ' ') {
            szRequest[i] = 0;
            artifacts.method = szRequest;
            artifacts.resource = szRequest + i + 1;
            debug_printf("got resource %s\n", artifacts.resource);
            break;
        }
    }

    get_header_value(headers,"x-port",&artifacts.port);
    get_header_value(headers,"host",&artifacts.host);

    char *authorization;

    if(get_header_value(headers,"authorization",&authorization)){
        debug_printf("authorization: %s\n", authorization);

        if(hawk_parse_artifacts(authorization,&artifacts)){
            debug_printf("host=[%s]\n",artifacts.host);
            debug_printf("port=[%s]\n",artifacts.port);
            debug_printf("method=[%s]\n",artifacts.method);
            debug_printf("resource=[%s]\n",artifacts.resource);
            debug_printf("id=[%s]\n",artifacts.id);
            debug_printf("ts=[%s]\n",artifacts.ts);
            debug_printf("nonce=[%s]\n",artifacts.nonce);
            debug_printf("ext=[%s]\n",artifacts.ext);
            debug_printf("mac=[%s]\n",artifacts.mac);

            return HTTP_200_OK;
        }
    }

    return HTTP_400_BAD_REQUEST;
}
int main(int argc, char **argv){
    debug_printf("oauth endpoint\n");
    EndpointEntry endpoints[] = {
        {HTTP_GET, "authorize", get_authorize, EP_PERMIT_UNAUTH},
        {HTTP_POST, "token", get_token, EP_PERMIT_UNAUTH},
        {HTTP_GET, 0, get_all, EP_PERMIT_UNAUTH},
    };

    debug_printf("executing oauth methods\n");
    return exec_endpoint(argc,argv,endpoints,ArrayCount(endpoints));
}
