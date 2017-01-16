// ============================================================================
// This is an Initialization script for the G-WAN Web Server (http://gwan.ch)
// ----------------------------------------------------------------------------
// init.c: This script is run at startup before G-WAN starts listening so you
//         have a chance to do initiatizations before any traffic can hit the
//         server.
//
//         Do whatever initialization you need to do here, like loading and
//         attaching data or an external database to the G-WAN US_SERVER_DATA
//         persistent pointer.
//
//         The list of the get_env() values that can be used from init.c or
//         main.c is: (get_env() calls with other values will be ignored)
//
//           US_SERVER_DATA   // global server pointer for user-defined data
//           SERVER_SOFTWARE  // "Server: G-WAN" HTTP response header
//           SCRIPT_TMO       // time-out in ms running a script
//           KALIVE_TMO       // time-out in ms for HTTP keep-alives
//           REQUEST_TMO      // time-out in ms waiting for request
//           MIN_SEND_SPEED   // send rate in bytes/sec (if < close)
//           MIN_READ_SPEED   // read rate in bytes/sec (if < close)
//           MAX_ENTITY_SIZE  // maximum POST entity size
//           USE_WWW_CACHE    // enable static  cache (default: off)
//           USE_CSP_CACHE    // enable servlet cache (default: off)
//           CACHE_ALL_WWW    // load all /www in cache (default: off)
//           USE_MINIFYING    // enable JS/CSS/HTML minifying (default: off)
//
//         Note that, unlike the optional Maintenance script (started later 
//         and run in its own thread so it can either stop or loop forever), 
//         this init.c script MUST return to let G-WAN start listening.
//
//         To avoid running the optional init.c script, rename it to anything 
//         else than 'init.c' ('init.c_' used by default, is fine).
// ============================================================================
#include "gwan.h"  // G-WAN API
#include <stdio.h> // puts()

int main(int argc, char *argv[])
{

   u8 *query_char = (u8*)get_env(argv, QUERY_CHAR);
   if(query_char) 
   {
      *query_char = '_'; 
   }

   return 0;
}

