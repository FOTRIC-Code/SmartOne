#include "ftp_client.h"

#include "libs/log/log.h"

#include <curl/curl.h>
#include <stdio.h>
#include <string.h>

#define FTP_DEFAULT_PORT 21
#define FTP_TEST_TIMEOUT_SEC 5L
#define FTP_UPLOAD_TIMEOUT_SEC 15L

typedef struct ftp_upload_buffer_t {
	const unsigned char *data;
	size_t size;
	size_t offset;
} ftp_upload_buffer_t;

static int ftp_config_valid(const ftp_conn_cfg_t *config)
{
	return config && config->ip[0];
}

static void ftp_build_url(const ftp_conn_cfg_t *config,
			  const char *remote_path, char *out,
			  size_t out_size)
{
	snprintf(out, out_size, "ftp://%s:%d/%s", config->ip,
		 config->port > 0 ? config->port : FTP_DEFAULT_PORT,
		 remote_path ? remote_path : "");
}

static void ftp_apply_credentials(CURL *curl,
				  const ftp_conn_cfg_t *config)
{
	if (config->enable_annoy) {
		curl_easy_setopt(curl, CURLOPT_USERNAME, "anonymous");
		curl_easy_setopt(curl, CURLOPT_PASSWORD, "");
	} else {
		curl_easy_setopt(curl, CURLOPT_USERNAME, config->user);
		curl_easy_setopt(curl, CURLOPT_PASSWORD, config->pwd);
	}
}

static void ftp_apply_tls(CURL *curl, const ftp_conn_cfg_t *config)
{
	if (!config->use_ssl)
		return;
	/* Holly uses explicit FTPS: connect with FTP, then upgrade via AUTH TLS. */
	curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
}

static size_t ftp_read_buffer(char *dst, size_t size, size_t count,
			      void *opaque)
{
	ftp_upload_buffer_t *buffer = opaque;
	size_t capacity;
	size_t remaining;
	size_t amount;

	if (!dst || !buffer || size == 0 || count == 0 ||
	    buffer->offset >= buffer->size)
		return 0;
	capacity = count > (size_t)-1 / size ? (size_t)-1 : size * count;
	remaining = buffer->size - buffer->offset;
	amount = capacity < remaining ? capacity : remaining;
	memcpy(dst, buffer->data + buffer->offset, amount);
	buffer->offset += amount;
	return amount;
}

int ftp_upload_buffer(const ftp_conn_cfg_t *config, const void *data,
		      size_t data_size, const char *remote_path)
{
	CURL *curl;
	CURLcode code;
	ftp_upload_buffer_t upload;
	char url[320];

	if (!ftp_config_valid(config) || !data || data_size == 0 ||
	    !remote_path || !remote_path[0])
		return -1;

	ftp_build_url(config, remote_path, url, sizeof(url));
	curl = curl_easy_init();
	if (!curl)
		return -1;
	upload.data = data;
	upload.size = data_size;
	upload.offset = 0;
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, ftp_read_buffer);
	curl_easy_setopt(curl, CURLOPT_READDATA, &upload);
	curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)data_size);
	curl_easy_setopt(curl, CURLOPT_FTP_CREATE_MISSING_DIRS,
			 CURLFTP_CREATE_DIR_RETRY);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, FTP_UPLOAD_TIMEOUT_SEC);
	ftp_apply_credentials(curl, config);
	ftp_apply_tls(curl, config);
	LOGI("FTP: uploading %zu bytes to %s", data_size, remote_path);
	code = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if (code != CURLE_OK) {
		LOGE("FTP: upload failed: %s", curl_easy_strerror(code));
		return -1;
	}
	LOGI("FTP: upload succeeded");
	return 0;
}

int ftp_test_login(const ftp_conn_cfg_t *config)
{
	CURL *curl;
	CURLcode code;
	char url[256];

	if (!ftp_config_valid(config))
		return -1;
	ftp_build_url(config, "", url, sizeof(url));
	curl = curl_easy_init();
	if (!curl)
		return -1;
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, FTP_TEST_TIMEOUT_SEC);
	ftp_apply_credentials(curl, config);
	ftp_apply_tls(curl, config);
	code = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	return code == CURLE_OK ? 0 : -1;
}
