#ifndef APP_SHARED_NETWORK_MANAGER_H
#define APP_SHARED_NETWORK_MANAGER_H

#include <stddef.h>

/*
 * Read-only view of the kernel's network state (addresses, routes, DNS).
 * Applying Wi-Fi configuration stays in main_app's wifi manager.
 */

#define NETWORK_MAX_IFACES 4
#define NETWORK_MAX_DNS 16
#define NETWORK_STR_LEN 64

/*
 * The AIC8800DC is a single-radio combo chip: hostapd (AP mode) and
 * wpa_supplicant (STA mode) both drive wlan0, so one interface name
 * covers every network mode of this product.
 */
#define NETWORK_IFACE_WLAN "wlan0"

typedef struct {
	int id;
	char name[NETWORK_STR_LEN];
	char ip[NETWORK_STR_LEN];
	char netmask[NETWORK_STR_LEN];
	char gateway[NETWORK_STR_LEN];
	int has_gateway;
	/*
	 * Layer-2 link (IFF_UP | IFF_RUNNING). On a station interface this
	 * stays 0 until wpa_supplicant associates, which is what separates a
	 * stale DHCP lease from a link that can actually carry traffic.
	 */
	int carrier;
} network_iface_t;

typedef struct {
	char server[NETWORK_STR_LEN];
} network_dns_t;

typedef struct {
	network_iface_t ifaces[NETWORK_MAX_IFACES];
	int iface_count;
	network_dns_t dns[NETWORK_MAX_DNS];
	int dns_count;
} network_state_t;

/**
 * Read every non-loopback IPv4 interface plus the resolver list.
 * @return 0 on success, -1 when no interface carries an IPv4 address
 */
int network_read_state(network_state_t *state);

/**
 * Read one interface by name. Unlike network_read_state() this also reports
 * interfaces that carry no IPv4 address yet (`ip` stays empty), so callers can
 * tell "no such interface" from "interface down" from "up but unaddressed".
 * No fork, no blocking I/O — safe to call from a request handler.
 * @return 0 when the interface exists, -1 otherwise (out is zeroed)
 */
int network_read_iface(const char *ifname, network_iface_t *out);

/**
 * Read MAC of ifname into "AA:BB:CC:DD:EE:FF".
 * @return 0 on success, -1 on failure
 */
int network_read_mac(const char *ifname, char *mac_buf, int buf_len);

#endif
