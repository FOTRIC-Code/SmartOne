#include "network_manager.h"

#include "libs/log/log.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

static int read_mac(const char *ifname, unsigned char mac[6])
{
	int fd;
	struct ifreq ifr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return -1;
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
	if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
		close(fd);
		return -1;
	}
	memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
	close(fd);
	return 0;
}

int network_read_mac(const char *ifname, char *mac_buf, int buf_len)
{
	unsigned char mac[6];

	if (!ifname || !mac_buf || buf_len < 18)
		return -1;
	if (read_mac(ifname, mac) != 0)
		return -1;
	snprintf(mac_buf, (size_t)buf_len, "%02X:%02X:%02X:%02X:%02X:%02X",
		 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return 0;
}

static void copy_in_addr(char *dst, size_t size, const struct sockaddr *sa)
{
	const struct sockaddr_in *in = (const struct sockaddr_in *)sa;

	if (!dst || size == 0 || !sa)
		return;
	if (!inet_ntop(AF_INET, &in->sin_addr, dst, (socklen_t)size))
		dst[0] = '\0';
}

static void read_gateway(network_iface_t *iface)
{
	FILE *fp = fopen("/proc/net/route", "r");
	char line[256];

	if (!fp)
		return;

	/* Skip the header row. */
	if (!fgets(line, sizeof(line), fp)) {
		fclose(fp);
		return;
	}

	while (fgets(line, sizeof(line), fp)) {
		char name[64];
		unsigned long dest, gw;
		struct in_addr addr;

		if (sscanf(line, "%63s %lx %lx", name, &dest, &gw) != 3)
			continue;
		if (dest != 0 || gw == 0 || strcmp(name, iface->name) != 0)
			continue;
		addr.s_addr = (in_addr_t)gw;
		if (inet_ntop(AF_INET, &addr, iface->gateway,
			      sizeof(iface->gateway)))
			iface->has_gateway = 1;
		break;
	}
	fclose(fp);
}

static void read_dns(network_state_t *state)
{
	FILE *fp = fopen("/etc/resolv.conf", "r");
	char line[256];

	if (!fp)
		return;

	while (fgets(line, sizeof(line), fp) &&
	       state->dns_count < NETWORK_MAX_DNS) {
		char ns[NETWORK_STR_LEN];

		if (sscanf(line, "nameserver %63s", ns) != 1)
			continue;
		strncpy(state->dns[state->dns_count].server, ns,
			NETWORK_STR_LEN - 1);
		state->dns_count++;
	}
	fclose(fp);
}

int network_read_state(network_state_t *state)
{
	struct ifaddrs *ifaddr, *ifa;
	int cnt = 0;

	if (!state)
		return -1;
	memset(state, 0, sizeof(*state));

	if (getifaddrs(&ifaddr) == -1)
		return -1;

	for (ifa = ifaddr; ifa && cnt < NETWORK_MAX_IFACES; ifa = ifa->ifa_next) {
		network_iface_t *n;

		if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
			continue;
		if (ifa->ifa_flags & IFF_LOOPBACK)
			continue;
		if (strchr(ifa->ifa_name, ':'))
			continue;

		n = &state->ifaces[cnt];
		n->id = cnt + 1;
		strncpy(n->name, ifa->ifa_name, NETWORK_STR_LEN - 1);
		copy_in_addr(n->ip, sizeof(n->ip), ifa->ifa_addr);
		if (ifa->ifa_netmask)
			copy_in_addr(n->netmask, sizeof(n->netmask),
				     ifa->ifa_netmask);
		if ((ifa->ifa_flags & IFF_UP) && (ifa->ifa_flags & IFF_RUNNING))
			n->carrier = 1;
		read_gateway(n);
		cnt++;
	}
	freeifaddrs(ifaddr);
	state->iface_count = cnt;

	read_dns(state);

	if (cnt == 0) {
		LOGW("No network interfaces found");
		return -1;
	}
	return 0;
}

int network_read_iface(const char *ifname, network_iface_t *out)
{
	struct ifaddrs *list = NULL;
	struct ifaddrs *ifa;
	int found = 0;

	if (!ifname || !out)
		return -1;

	memset(out, 0, sizeof(*out));
	if (getifaddrs(&list) != 0)
		return -1;

	for (ifa = list; ifa; ifa = ifa->ifa_next) {
		if (!ifa->ifa_name || strcmp(ifa->ifa_name, ifname) != 0)
			continue;

		/* Flags come with every family entry, including AF_PACKET. */
		found = 1;
		if ((ifa->ifa_flags & IFF_UP) && (ifa->ifa_flags & IFF_RUNNING))
			out->carrier = 1;

		if (out->ip[0] || !ifa->ifa_addr ||
		    ifa->ifa_addr->sa_family != AF_INET)
			continue;

		copy_in_addr(out->ip, sizeof(out->ip), ifa->ifa_addr);
		if (ifa->ifa_netmask)
			copy_in_addr(out->netmask, sizeof(out->netmask),
				     ifa->ifa_netmask);
	}
	freeifaddrs(list);

	if (!found)
		return -1;
	strncpy(out->name, ifname, NETWORK_STR_LEN - 1);
	read_gateway(out);
	return 0;
}
