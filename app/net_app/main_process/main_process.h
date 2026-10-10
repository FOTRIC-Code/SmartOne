#ifndef NET_APP_MAIN_PROCESS_H
#define NET_APP_MAIN_PROCESS_H

#include "net_common/net_common.h"
#include "net_common/net_enums.h"

int main_process_init(void);
void main_process_uninit(void);
void main_process_request_task(net_enum_task_type_t task);
int main_process_submit_alarm_event(const alarm_event_t *event);
void main_process_submit_body_temp_sample(float temperature,
					  uint64_t timestamp_us);

#endif
