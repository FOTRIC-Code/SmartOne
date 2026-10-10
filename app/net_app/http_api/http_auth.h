#ifndef NET_APP_HTTP_AUTH_H
#define NET_APP_HTTP_AUTH_H

#include "libs/mongoose/mongoose.h"

/* Returns 0 if allowed, -1 if rejected (response already sent). */
int http_auth_gate(struct mg_connection *c, struct mg_http_message *hm,
		   const char *path);

#endif
