#ifndef NET_APP_CONFIG_TYPES_H
#define NET_APP_CONFIG_TYPES_H

#include "net_common/net_common.h"

typedef hsi_info_t hsi_info_cfg;
typedef time_info_t time_info_cfg;
typedef ntp_info_t ntp_info_cfg;
typedef port_info_t port_info_cfg;
typedef network_info_t network_info_cfg;

/* From oem.json (Product_Company / Product_Brand / Language_DefaultLanguage). */
typedef struct {
	char company[NET_LEN_128];
	char brand[NET_LEN_128];
	char language[NET_LEN_32];
} recipe_info_cfg;

typedef provision_info_t provision_info_cfg;
typedef stream_info_t stream_info_cfg;

typedef struct seal_param_cfg {
	int sealed;
} seal_param_cfg;

typedef intrusion_alarm_t intrusion_alarm_cfg;
typedef high_temp_alarm_t high_temp_alarm_cfg;
typedef body_temp_alarm_t body_temp_alarm_cfg;
typedef alarm_trigger_t alarm_trigger_cfg;
typedef ftp_info_t ftp_info_cfg;
typedef webhook_info_t webhook_info_cfg;
typedef thermal_settings_t thermal_settings_cfg;
typedef image_settings_t image_settings_cfg;

typedef struct {
	int enabled;
	int level;
	int max_file_size;
	char file_path[NET_LEN_128];
} log_cfg;

#endif
