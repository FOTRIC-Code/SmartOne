#ifndef NET_APP_LIBS_FTP_FTP_CLIENT_H
#define NET_APP_LIBS_FTP_FTP_CLIENT_H

#include <stddef.h>

typedef struct ftp_conn_cfg_t {
	char ip[64];
	int port;
	char user[64];
	char pwd[128];
	int use_ssl;
	int enable_annoy;
} ftp_conn_cfg_t;

int ftp_upload_buffer(const ftp_conn_cfg_t *config, const void *data,
		      size_t data_size, const char *remote_path);
int ftp_test_login(const ftp_conn_cfg_t *config);

#endif
