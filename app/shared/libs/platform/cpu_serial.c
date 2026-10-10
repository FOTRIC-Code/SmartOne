#include "cpu_serial.h"

#include "libs/log/log.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define NVMEM_DIR "/sys/bus/nvmem/devices"
#define OTP_ID_OFFSET 0x0a
#define OTP_ID_LEN 16

static int path_is_rockchip_otp(const char *name)
{
	return name && strncmp(name, "rockchip-otp", 12) == 0;
}

static int open_otp_nvmem(void)
{
	DIR *dir;
	struct dirent *ent;
	char path[256];
	int fd = -1;

	dir = opendir(NVMEM_DIR);
	if (!dir) {
		LOGE("opendir %s failed: %s", NVMEM_DIR, strerror(errno));
		return -1;
	}

	while ((ent = readdir(dir)) != NULL) {
		if (!path_is_rockchip_otp(ent->d_name))
			continue;
		snprintf(path, sizeof(path), "%s/%s/nvmem", NVMEM_DIR,
			 ent->d_name);
		fd = open(path, O_RDONLY);
		if (fd >= 0) {
			LOGI("opened OTP nvmem %s", path);
			break;
		}
		LOGW("open %s failed: %s", path, strerror(errno));
	}

	closedir(dir);
	return fd;
}

static void bytes_to_hex(const unsigned char *in, size_t len, char *out)
{
	static const char hex[] = "0123456789abcdef";
	size_t i;

	for (i = 0; i < len; ++i) {
		out[i * 2] = hex[(in[i] >> 4) & 0xf];
		out[i * 2 + 1] = hex[in[i] & 0xf];
	}
	out[len * 2] = '\0';
}

int cpu_serial_read(char *out, size_t out_size)
{
	unsigned char id[OTP_ID_LEN];
	ssize_t n;
	int fd;

	if (!out || out_size < CPU_SERIAL_HEX_LEN + 1)
		return -1;

	fd = open_otp_nvmem();
	if (fd < 0)
		return -1;

	if (lseek(fd, OTP_ID_OFFSET, SEEK_SET) != (off_t)OTP_ID_OFFSET) {
		LOGE("lseek OTP id@a failed: %s", strerror(errno));
		close(fd);
		return -1;
	}

	n = read(fd, id, OTP_ID_LEN);
	close(fd);
	if (n != (ssize_t)OTP_ID_LEN) {
		LOGE("read OTP id@a got %zd bytes (want %d)", n,
		     OTP_ID_LEN);
		return -1;
	}

	bytes_to_hex(id, OTP_ID_LEN, out);
	return 0;
}
