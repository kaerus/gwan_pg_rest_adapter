printf("\n---------- RUN ---------\n");


xbuf_t *reply = get_reply(argv);


xbuf_t *request = (xbuf_t *)get_env(argv, READ_XBUF);


void **pdata = (void**)get_env(argv, US_REQUEST_DATA);


if (!pdata[0]) { // no request data yet, send upgrade to websocket

    char *upgrade = xbuf_findstr(request, "\r\nUpgrade: websocket\r\n"); // FF only sends this ; chrome also sends "Connection: Upgrade\r\n" afterwards



    if (upgrade != NULL) { // correct upgrade header found?
	const char keyHeader[] ="\r\nSec-WebSocket-Key: ";

	char *key = xbuf_findstr(request, (char *)keyHeader);

	if (key != NULL && (key += sizeof(keyHeader) - 1) != NULL && (request->len - (u32)(key - request->ptr)) >= (u32)23) { // correct key header found? + sanity check
	    char websocketGUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"; // 8 + 4 + 4 + 4 + 12 = 32 + 4 bytes for '-' = 36

	    const char data[] = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %20B\r\n\r\n";

	    // 36 + 24
	    char keyCompose[60];
	    strncpy(keyCompose, key, 24);
	    strncpy(keyCompose + 24, websocketGUID, 36);

	    u8 sha[20];
	    sha1_t ctx;
	    sha1_init(&ctx);
	    sha1_add(&ctx, (u8 *)keyCompose, 60);

	    sha1_end(&ctx, sha);


	    xbuf_xcat(reply, (char *)data, sha);

	    pdata[0] = (void*)1;

	    printf("Init.");

	    return RC_NOHEADERS + RC_STREAMING; // don't build headers automatically
	}
    }


 } else { // websocket connection here


    const unsigned char websocketTerm[2] = { 0x00, 0xFF }; // websocket close = opcode 0x8
    printf("Streaming!\n"); // TODO: next find way to get next user input (best would be to only wake up on user input or if we have something to send?)

    char *buf = alloca(2);
    buf[0] = 0; buf[1] = 0;

    pdata[0]++;

    if ((int)(pdata[0]) >= 20) {
	xbuf_ncat(reply, (char *)websocketTerm, 2);

	printf("Fin.\n");

	pdata[0] = 0; // cleanup

	return RC_NOHEADERS;
    }

    return RC_NOHEADERS + RC_STREAMING;
 }

xbuf_cat(reply, "test");

return 403;
