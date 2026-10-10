#ifndef NET_APP_ALARM_TRANSFER_H
#define NET_APP_ALARM_TRANSFER_H

#include "net_common/net_common.h"
#include "net_common/net_common.h"

int alarm_transfer_init(void);
void alarm_transfer_uninit(void);
int alarm_ftp_test(const ftp_info_t *config);
int alarm_ftp_upload(const ftp_info_t *config, const alarm_event_t *event);
int alarm_webhook_post(const webhook_info_t *config,
		       const alarm_event_t *event);
int body_temp_sample_webhook_post(const webhook_info_t *config,
				  float temperature, uint64_t timestamp_us);

#endif
