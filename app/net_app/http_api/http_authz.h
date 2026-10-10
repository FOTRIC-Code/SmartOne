#ifndef NET_APP_HTTP_AUTHZ_H
#define NET_APP_HTTP_AUTHZ_H

/* Route trust levels (Cube provisioning model). */
#define HTTP_AUTHZ_LEVEL_OPEN 0   /* always allowed */
#define HTTP_AUTHZ_LEVEL_AUTH 1   /* auth when provisioned */
#define HTTP_AUTHZ_UNREGISTERED (-1)

/*
 * Look up minimum trust level for path + HTTP method.
 * Returns HTTP_AUTHZ_UNREGISTERED if the route is not registered (fail-closed).
 */
int http_authz_required_level(const char *path, const char *method);

#endif
