#include "http_auth.h"

#include "http_api_common.h"
#include "http_authz.h"
#include "net_common/net_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void reply_auth_error(struct mg_connection *c, int code,
			     const char *message)
{
	char *resp = create_error(code, message);

	mg_http_reply(c, 200,
		      "Content-Type: application/json\r\n"
		      "Access-Control-Allow-Origin: *\r\n",
		      "%s",
		      resp ? resp :
			     (code == EC_FORBIDDEN ?
				      "{\"code\":-403,\"message\":\"Forbidden\"}" :
				      "{\"code\":-401,\"message\":\"Unauthorized\"}"));
	free(resp);
}

static const char *method_string(struct mg_http_message *hm)
{
	static char buf[16];

	if (!hm || !hm->method.buf || hm->method.len == 0)
		return "";
	snprintf(buf, sizeof(buf), "%.*s", (int)hm->method.len, hm->method.buf);
	return buf;
}

int http_auth_gate(struct mg_connection *c, struct mg_http_message *hm,
		   const char *path)
{
	const char *method = method_string(hm);
	int required = http_authz_required_level(path, method);
	char user[64] = { 0 };
	char pass[64] = { 0 };

	if (required == HTTP_AUTHZ_UNREGISTERED) {
		reply_auth_error(c, EC_FORBIDDEN, "Forbidden");
		return -1;
	}

	if (required == HTTP_AUTHZ_LEVEL_OPEN)
		return 0;

	/* SoftAP / first-provision: allow without auth when unprovisioned. */
	if (g_config.provision.provision_state != NET_PROVISION_STATE_PROVISIONED)
		return 0;

	mg_http_creds(hm, user, sizeof(user), pass, sizeof(pass));
	if (user[0] != '\0')
		return 0;

	reply_auth_error(c, EC_UNAUTHORIZED, "Unauthorized");
	return -1;
}
