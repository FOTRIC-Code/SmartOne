#ifndef NET_APP_CONFIG_MANAGER_H
#define NET_APP_CONFIG_MANAGER_H

#include "config_types.h"

/* Full config paths (Holly-style literals). */
#define NET_CFG_HSI_PATH "/etc/cfg/device/hsi/config.json"
#define NET_CFG_TIME_PATH "/etc/cfg/device/time/config.json"
#define NET_CFG_NTP_PATH "/etc/cfg/device/ntp/config.json"
#define NET_CFG_NETWORK_PATH "/etc/cfg/device/network/config.json"
#define NET_CFG_PORT_PATH "/etc/cfg/device/port/config.json"
#define NET_CFG_LOG_PATH "/etc/cfg/device/log/config.json"
#define NET_CFG_PROVISION_PATH "/etc/cfg/provision/state/config.json"
#define NET_CFG_SEAL_PATH "/etc/cfg/fab/status/config.json"
#define NET_CFG_RECIPE_PATH "/etc/cfg/sensor/recipe_file/oem/oem.json"
#define NET_CFG_RECIPE_FOLDER "/etc/cfg/sensor/recipe_file"
#define NET_CFG_ALARM_INTRUSION_PATH "/etc/cfg/alarm/intrusion.json"
#define NET_CFG_ALARM_HIGH_TEMP_PATH "/etc/cfg/alarm/high_temp.json"
#define NET_CFG_ALARM_BODY_TEMP_PATH "/etc/cfg/alarm/body_temp.json"
#define NET_CFG_ALARM_TRIGGER_PATH "/etc/cfg/alarm/trigger.json"
#define NET_CFG_RECORD_CONFIG_PATH "/etc/cfg/alarm/record_config.json"
#define NET_CFG_ALARM_TRIGGER_INTRUSION_PATH "/etc/cfg/alarm/trigger_intrusion.json"
#define NET_CFG_ALARM_TRIGGER_HIGH_TEMP_PATH "/etc/cfg/alarm/trigger_high_temp.json"
#define NET_CFG_ALARM_TRIGGER_BODY_TEMP_PATH "/etc/cfg/alarm/trigger_body_temp.json"
#define NET_CFG_FTP_PATH "/etc/cfg/device/ftp/config.json"
#define NET_CFG_WEBHOOK_PATH "/etc/cfg/device/webhook/config.json"
#define NET_CFG_THERMAL_PATH "/etc/cfg/thermal/config.json"
#define NET_CFG_IMAGE_PATH "/etc/cfg/device/image/config.json"

int config_init(void);
void config_uninit(void);

hsi_info_cfg *config_get_hsi_info(void);
int config_set_hsi_info(const hsi_info_cfg *info);

recipe_info_cfg *config_get_recipe_info(void);

time_info_cfg *config_get_time_info(void);
int config_set_time_info(const time_info_cfg *info);

ntp_info_cfg *config_get_ntp_server(void);
int config_set_ntp_server(const ntp_info_cfg *info);

int config_get_networks(network_info_cfg *networks, int *count);
network_info_cfg *config_get_network(int id);
int config_set_network(int id, const network_info_cfg *network);
int config_set_networks_batch(const network_info_cfg *networks, int count);

provision_info_cfg *config_get_provision(void);
int config_reset_factory_defaults(void);

seal_param_cfg *config_get_seal_param(void);
int config_set_seal_param(const seal_param_cfg *seal);

port_info_cfg *config_get_port_info(void);
int config_set_port_info(const port_info_cfg *info);

intrusion_alarm_cfg *config_get_intrusion_alarm(void);
int config_set_intrusion_alarm(const intrusion_alarm_cfg *cfg);
high_temp_alarm_cfg *config_get_high_temp_alarm(void);
int config_set_high_temp_alarm(const high_temp_alarm_cfg *cfg);
body_temp_alarm_cfg *config_get_body_temp_alarm(void);
int config_set_body_temp_alarm(const body_temp_alarm_cfg *cfg);
alarm_trigger_cfg *config_get_alarm_trigger(int type);
int config_set_alarm_trigger(int type, const alarm_trigger_cfg *cfg);
ftp_info_cfg *config_get_ftp_info(void);
int config_set_ftp_info(const ftp_info_cfg *cfg);
webhook_info_cfg *config_get_webhook(void);
int config_set_webhook(const webhook_info_cfg *cfg);
thermal_settings_cfg *config_get_thermal_settings(void);
int config_set_thermal_settings(const thermal_settings_cfg *cfg);
image_settings_cfg *config_get_image_settings(void);
int config_set_image_settings(const image_settings_cfg *cfg);
int config_copy_alarm_linkage(int type, alarm_trigger_cfg *trigger,
			      ftp_info_cfg *ftp, webhook_info_cfg *webhook);

log_cfg *config_get_log(void);
int config_set_log(const log_cfg *cfg);


#endif
