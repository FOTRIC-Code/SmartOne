#include "http_api_common.h"
#include "libs/file/file_util.h"
#include "libs/json/json_util.h"
#include "libs/log/log.h"
#include "libs/network/network_manager.h"
#include "libs/platform/command.h"
#include "libs/upgrade/app_slot_upgrade.h"
#include "main_process/main_process.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* ==================== IP conflict detection (ported from Holly) ==================== */

static int network_check_ip_conflict(const char *ifname, const char *ip)
{
	char cmd[256];
	int st, rc;

	if (!ifname || !ifname[0] || !ip || !ip[0])
		return -1;

	snprintf(cmd, sizeof(cmd),
		 "arping -D -I %s -c 2 -w 3 %s >/dev/null 2>&1",
		 ifname, ip);
	LOGI("IP conflict check: %s", cmd);

	st = system(cmd);
	if (st < 0) {
		LOGW("arping exec failed, allow setting");
		return -1;
	}

	rc = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
	if (rc == 1) {
		LOGW("IP conflict detected: %s is already in use on %s",
		     ip, ifname);
		return 1;
	}
	return 0;
}

char *handle_system_device_info_get(void)
{
	hsi_info_cfg *hsi_info = config_get_hsi_info();
	recipe_info_cfg *recipe_info = config_get_recipe_info();
	device_info_dto device_dto;

	if (!hsi_info || !recipe_info)
		return create_error(EC_FAIL, "Config unavailable");

	device_info_from_hsi_info_to_dto(hsi_info, recipe_info, &device_dto);
	return create_response(EC_OK, "OK",
			       cfg_to_json(&device_dto, device_info_dto_fields,
					   device_info_dto_fc));
}

char *handle_system_device_info_put(const char *body, int len)
{
	device_info_dto dto;
	hsi_info_cfg *hsi_info;

	memset(&dto, 0, sizeof(dto));
	if (parse_body_to_dto(body, len, &dto, device_info_dto_fields,
			      device_info_dto_fc) != 0)
		return create_error(EC_INVALID_PARAM, "Invalid device info payload");
	if (str_contains_windows_folder_illegal_chars(dto.name))
		return create_error(EC_INVALID_PARAM,
				    "Device name contains invalid characters");

	hsi_info = config_get_hsi_info();
	if (!hsi_info)
		return create_error(EC_FAIL, "Failed to load HSI info");

	snprintf(hsi_info->name, sizeof(hsi_info->name), "%s", dto.name);

	if (config_set_hsi_info(hsi_info) != 0)
		return create_error(EC_FAIL, "Failed to save device info");

	return create_response(EC_OK, "Device info updated", NULL);
}

char *handle_system_restart(void)
{
	main_process_request_task(NET_TASK_DEVICE_RESTART);
	return create_response(EC_OK, "Restarting", NULL);
}

char *handle_system_reset(void)
{
	if (config_reset_factory_defaults() != 0)
		return create_error(EC_FAIL, "Factory reset failed");
	main_process_request_task(NET_TASK_DEVICE_RESTART);
	return create_response(EC_OK, "Device reset to factory defaults", NULL);
}

char *handle_system_upgrade(const char *filepath)
{
	app_slot_upgrade_result_t result;
	char err[128];

	memset(&result, 0, sizeof(result));
	err[0] = '\0';
	if (app_slot_upgrade_apply(filepath, &result, err, sizeof(err)) != 0)
		return create_error(EC_FAIL, err[0] ? err : "Upgrade failed");

	{
		hsi_info_cfg *cur = config_get_hsi_info();
		hsi_info_cfg next_hsi;

		if (cur) {
			next_hsi = *cur;
			snprintf(next_hsi.firmware_version,
				 sizeof(next_hsi.firmware_version), "%s",
				 result.version);
			if (result.firmware_name[0])
				snprintf(next_hsi.firmware_name,
					 sizeof(next_hsi.firmware_name), "%s",
					 result.firmware_name);
			config_set_hsi_info(&next_hsi);
		}
	}

	LOGI("Upgrade accepted, version=%s", result.version);
	main_process_request_task(NET_TASK_DEVICE_RESTART);
	return create_response(EC_OK, "Upgrade accepted", NULL);
}

char *handle_system_time_get(void)
{
	HANDLE_GET(time_info_cfg, time_info_dto, config_get_time_info,
		   time_info_to_dto, time_info_dto_fields, time_info_dto_fc);
}

char *handle_system_time_put(const char *body, int len)
{
	HANDLE_PUT_VALID_BODY(time_info_dto, time_info_cfg, config_get_time_info,
			      time_info_to_dto, time_info_from_dto,
			      config_set_time_info, time_info_dto_fields,
			      time_info_dto_fc, body, len, time_info_dto_is_valid,
			      "Time updated");
}

char *handle_system_ntp_server_get(void)
{
	HANDLE_GET(ntp_info_cfg, ntp_info_dto, config_get_ntp_server,
		   ntp_info_to_dto, ntp_info_dto_fields, ntp_info_dto_fc);
}

char *handle_system_ntp_server_put(const char *body, int len)
{
	HANDLE_PUT_VALID_BODY(ntp_info_dto, ntp_info_cfg, config_get_ntp_server,
			      ntp_info_to_dto, ntp_info_from_dto,
			      config_set_ntp_server, ntp_info_dto_fields,
			      ntp_info_dto_fc, body, len, ntp_info_dto_is_valid,
			      "NTP updated");
}

/*
 * A DHCP lease only ever exists in the kernel, never in config.json, so the
 * stored entry stays empty while the device is reachable on a real address.
 * Overlay what wlan0 actually carries before reporting it.
 */
static void network_overlay_runtime(network_info_cfg *net)
{
	network_iface_t link;

	if (!net)
		return;

	if (g_config.provision.network_mode == NET_NETWORK_MODE_AP) {
		snprintf(net->ip, sizeof(net->ip), "%s",
			 g_config.provision.http_host[0] ?
				 g_config.provision.http_host :
				 "192.168.4.1");
		net->dhcp = 0;
		snprintf(net->netmask, sizeof(net->netmask), "255.255.255.0");
		return;
	}

	if (!net->dhcp)
		return;
	if (network_read_iface(NETWORK_IFACE_WLAN, &link) != 0 || !link.ip[0])
		return;

	snprintf(net->ip, sizeof(net->ip), "%s", link.ip);
	if (link.netmask[0])
		snprintf(net->netmask, sizeof(net->netmask), "%s",
			 link.netmask);
	if (link.has_gateway) {
		snprintf(net->gateway[0], sizeof(net->gateway[0]), "%s",
			 link.gateway);
		net->gateway_count = 1;
	}
}

char *handle_system_networks_get(void)
{
	network_info_cfg networks[NET_MAX_NETWORKS];
	int count = 0;

	config_get_networks(networks, &count);
	if (count > 0)
		network_overlay_runtime(&networks[0]);
	return create_response(EC_OK, "OK", networks_to_json(networks, count));
}

char *handle_system_network_get(int id)
{
	network_info_cfg *net = config_get_network(id);
	network_info_cfg live;
	network_info_dto dto;

	if (!net)
		return create_error(EC_FAIL, "Network not found");
	live = *net;
	network_overlay_runtime(&live);
	network_info_to_dto(&live, &dto);
	return create_response(EC_OK, "OK",
			       cfg_to_json(&dto, network_info_dto_fields,
					   network_info_dto_fc));
}

char *handle_system_network_put(int id, const char *body, int len)
{
	network_info_dto dto;
	network_info_cfg cfg;
	network_info_cfg *cur = config_get_network(id);

	if (g_config.provision.network_mode != NET_NETWORK_MODE_STA)
		return create_error(EC_NOT_SUPPORTED, "ap_ip_readonly");
	if (!cur)
		return create_error(EC_FAIL, "Network not found");
	network_info_to_dto(cur, &dto);
	if (parse_body_to_dto(body, len, &dto, network_info_dto_fields,
			      network_info_dto_fc) != 0 ||
	    !network_info_dto_is_valid(&dto))
		return create_error(EC_INVALID_PARAM, "Invalid network info");
	if (!dto.dhcp && dto.ip[0]) {
		int conflict = network_check_ip_conflict(dto.name, dto.ip);
		if (conflict == 1)
			return create_error(EC_INVALID_PARAM,
					    "IP address conflict detected on LAN");
	}
	network_info_from_dto(&dto, &cfg);
	if (config_set_network(id, &cfg) != 0)
		return create_error(EC_FAIL, "Failed to save network");
	return create_response(EC_OK, "Network updated", NULL);
}

char *handle_system_status_get(void)
{
	system_status_dto dto;

	system_status_to_dto(&dto);
	return create_response(EC_OK, "OK",
			       cfg_to_json(&dto, system_status_dto_fields,
					   system_status_dto_fc));
}

char *handle_system_port_get(void)
{
	HANDLE_GET(port_info_cfg, port_info_dto, config_get_port_info,
		   port_info_to_dto, port_info_dto_fields, port_info_dto_fc);
}

char *handle_system_port_put(const char *body, int len)
{
	HANDLE_PUT_VALID_BODY(port_info_dto, port_info_cfg, config_get_port_info,
			      port_info_to_dto, port_info_from_dto,
			      config_set_port_info, port_info_dto_fields,
			      port_info_dto_fc, body, len, port_info_dto_is_valid,
			      "Port updated");
}

char *handle_system_networks_put(const char *body, int len)
{
	network_info_cfg nets[NET_MAX_NETWORKS];
	int count = 0;
	int i;

	if (g_config.provision.network_mode != NET_NETWORK_MODE_STA)
		return create_error(EC_NOT_SUPPORTED, "ap_ip_readonly");
	if (networks_from_json(body, len, nets, NET_MAX_NETWORKS, &count) != 0)
		return create_error(EC_INVALID_PARAM, "Invalid networks payload");
	for (i = 0; i < count; ++i) {
		if (!nets[i].dhcp && nets[i].ip[0]) {
			int conflict = network_check_ip_conflict(
				nets[i].name, nets[i].ip);
			if (conflict == 1)
				return create_error(EC_INVALID_PARAM,
					"IP address conflict detected on LAN");
		}
	}
	if (config_set_networks_batch(nets, count) != 0)
		return create_error(EC_FAIL, "Failed to save network settings");
	return create_response(EC_OK, "Networks updated", NULL);
}

char *handle_discovery_recover_dhcp(void)
{
	network_info_cfg networks[NET_MAX_NETWORKS];
	network_info_cfg cfg;
	network_info_dto dto;
	int count = 0;

	config_get_networks(networks, &count);
	if (count <= 0) {
		LOGE("recover_dhcp: no network interface in config");
		return create_error(EC_FAIL, "No network interface");
	}

	cfg = networks[0];
	LOGI("recover_dhcp: enable dhcp on id=%d name=%s (was dhcp=%d ip=%s)",
	     cfg.id, cfg.name, cfg.dhcp, cfg.ip);
	if (cfg.dhcp) {
		LOGI("recover_dhcp: already dhcp id=%d name=%s", cfg.id,
		     cfg.name);
		return create_response(EC_OK, "DHCP already enabled", NULL);
	}
	cfg.dhcp = 1;
	network_info_to_dto(&cfg, &dto);
	if (!network_info_dto_is_valid(&dto)) {
		LOGE("recover_dhcp: invalid network config id=%d name=%s",
		     cfg.id, cfg.name);
		return create_error(EC_INVALID_PARAM,
				    "Invalid network config");
	}
	if (config_set_network(cfg.id, &cfg) != 0) {
		LOGE("recover_dhcp: config_set_network(id=%d) failed", cfg.id);
		return create_error(EC_FAIL, "Failed to set DHCP");
	}
	LOGI("recover_dhcp: dhcp enabled ok id=%d name=%s", cfg.id, cfg.name);
	return create_response(EC_OK, "DHCP enabled", NULL);
}
