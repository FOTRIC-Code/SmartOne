#include "ble/ble.h"

#include "ble/ble_advertiser.h"
#include "ble_config.h"
#include "libs/log/log.h"

#include <stdlib.h>
#include <unistd.h>

void ble_init(void)
{
	net_ble_advertisement_t adv = {
		.device_id = g_ble_config.hsi.id,
		.product_id = g_ble_config.hsi.brand,
		.model = g_ble_config.hsi.model,
		.serial_no = g_ble_config.hsi.serial_no,
		.hotspot_ssid = g_ble_config.provision.hotspot_ssid,
		.http_host = g_ble_config.provision.http_host[0] ?
				     g_ble_config.provision.http_host :
				     "192.168.4.1",
		.http_port_no = g_ble_config.provision.http_port_no ?
					g_ble_config.provision.http_port_no :
					BLE_DEFAULT_HTTP_PORT,
	};

	if (net_ble_advertiser_start(&adv) != 0)
		LOGW("BLE advertiser start failed");
}

void ble_poll(void)
{
	net_ble_poll();
}

void ble_poll_commands(void)
{
	net_ble_poll_commands();
}

bool ble_is_running(void)
{
	return net_ble_is_running();
}

void ble_notify_provision_result(bool success, const char *ip)
{
	net_ble_notify_provision_result(success, ip);
}

/*
 * AIC8800DC combo-chip coexistence helpers.
 *
 * The AIC8800DC shares a single radio between Wi-Fi and BT.  When bluetoothd
 * is active (advertising / GATT connections), the Wi-Fi data plane stalls
 * (TX/RX = 0).  Stopping BLE and bluetoothd before touching Wi-Fi, then
 * optionally restarting them afterwards, works around this limitation.
 */

void ble_stop(void)
{
	/*
	 * AIC8800DC combo-chip: just kill bluetoothd and power-down hci0.
	 *
	 * Do NOT call net_ble_advertiser_stop() here.  That function does
	 * a D-Bus / librkwifibt clean-shutdown which keeps HCI file
	 * descriptors open inside this process — enough to keep the
	 * shared radio's BT side active and stall the Wi-Fi data plane.
	 *
	 * The brute-force kill mirrors the manual test that works:
	 *   killall main_app bluetoothd → wifi-connect.sh → OK
	 */
	LOGI("ble_stop: killing bluetoothd + hci0 down (combo-chip coexistence)");
	(void)system("killall -9 bluetoothd >/dev/null 2>&1");
	(void)system("hciconfig hci0 down >/dev/null 2>&1");
	sleep(2);
}

void ble_restart(void)
{
	LOGI("ble_restart: bringing BLE stack back up");
	(void)system("/etc/init.d/S40bluetoothd start >/dev/null 2>&1");
	sleep(2);
	ble_init();
}
