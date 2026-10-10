#ifndef NET_APP_MAIN_PROCESS_TASKS_H
#define NET_APP_MAIN_PROCESS_TASKS_H

#include "net_common/net_common.h"

int main_task_ntp_sync(void);
int main_task_device_restart(void);
int main_task_alarm_ftp_upload(const alarm_event_t *event);
int main_task_alarm_webhook(const alarm_event_t *event);
int main_task_alarm_record(const alarm_event_t *event);
int main_task_body_temp_compose(const alarm_event_t *event);
int main_task_http_rebind(void);

#endif
