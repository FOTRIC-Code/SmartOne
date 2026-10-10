#include "file_util.h"

#include "libs/log/log.h"
#include "libs/platform/command.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void file_mkdir_p(const char *path)
{
	char tmp[512];
	char *p;

	if (!path || !path[0])
		return;
	snprintf(tmp, sizeof(tmp), "%s", path);
	for (p = tmp + 1; *p; ++p) {
		if (*p == '/') {
			*p = '\0';
			mkdir(tmp, 0755);
			*p = '/';
		}
	}
	mkdir(tmp, 0755);
}

int create_dir_recursive(const char *path)
{
	if (!path || !path[0])
		return -1;
	file_mkdir_p(path);
	return 0;
}

int file_exists(const char *path)
{
	return path && access(path, F_OK) == 0;
}

int file_ensure_dir(const char *file_path)
{
	char dir[512];
	char *last_slash;

	if (!file_path || file_path[0] == '\0')
		return -1;

	snprintf(dir, sizeof(dir), "%s", file_path);
	last_slash = strrchr(dir, '/');
	if (!last_slash)
		return 0;
	if (*(last_slash + 1) == '\0')
		return -1;
	*last_slash = '\0';
	if (dir[0] == '\0')
		return 0;
	file_mkdir_p(dir);
	return 0;
}

int file_write(const char *path, const char *data)
{
	FILE *fp;

	if (!path || !data)
		return -1;
	file_ensure_dir(path);
	fp = fopen(path, "w");
	if (!fp)
		return -1;
	if (fputs(data, fp) == EOF) {
		fclose(fp);
		return -1;
	}
	fclose(fp);
	return 0;
}

int file_read(const char *path, char **out_buf, size_t *out_len)
{
	struct stat st;
	FILE *fp;
	char *buf;
	size_t read_len;

	if (!path || !out_buf || !out_len)
		return -1;
	if (stat(path, &st) != 0)
		return -1;
	fp = fopen(path, "r");
	if (!fp)
		return -1;
	buf = (char *)malloc((size_t)st.st_size + 1);
	if (!buf) {
		fclose(fp);
		return -1;
	}
	read_len = fread(buf, 1, (size_t)st.st_size, fp);
	buf[read_len] = '\0';
	fclose(fp);
	*out_buf = buf;
	*out_len = read_len;
	return 0;
}

static int file_remove_tree_impl(const char *path, int keep_root)
{
	struct stat st;

	if (!path || !path[0])
		return -1;
	if (lstat(path, &st) != 0)
		return errno == ENOENT ? 0 : -1;

	if (S_ISDIR(st.st_mode)) {
		DIR *dir = opendir(path);
		struct dirent *ent;
		char child[512];
		int ret = 0;

		if (!dir)
			return -1;
		while ((ent = readdir(dir)) != NULL) {
			if (strcmp(ent->d_name, ".") == 0 ||
			    strcmp(ent->d_name, "..") == 0)
				continue;
			snprintf(child, sizeof(child), "%s/%s", path,
				 ent->d_name);
			if (file_remove_tree_impl(child, 0) != 0)
				ret = -1;
		}
		closedir(dir);
		if (ret == 0 && !keep_root && rmdir(path) != 0)
			return -1;
		return ret;
	}

	return unlink(path) == 0 ? 0 : -1;
}

int file_remove_tree(const char *path)
{
	return file_remove_tree_impl(path, 0);
}

int file_remove_tree_contents(const char *path)
{
	return file_remove_tree_impl(path, 1);
}

void file_remove_path_best_effort(const char *path)
{
	if (!path || !path[0])
		return;
	if (!file_exists(path))
		return;
	if (file_remove_tree(path) == 0) {
		LOGI("removed %s", path);
		return;
	}
	if (unlink(path) == 0) {
		LOGI("unlinked %s", path);
		return;
	}
	LOGW("failed to remove %s: %s", path, strerror(errno));
}

int file_chmod(const char *path, mode_t mode)
{
	if (!path || !path[0])
		return -1;
	return chmod(path, mode);
}

long file_size(const char *path)
{
	struct stat st;

	if (!path || stat(path, &st) != 0)
		return -1;
	return (long)st.st_size;
}

int file_extract_compressed_tar(const char *archive_path,
				const char *output_dir)
{
	char cmd[768];

	if (!archive_path || !output_dir)
		return -1;
	create_dir_recursive(output_dir);
	snprintf(cmd, sizeof(cmd),
		 "tar -xzf '%s' -C '%s' 2>/dev/null || "
		 "tar -xf '%s' -C '%s' 2>/dev/null",
		 archive_path, output_dir, archive_path, output_dir);
	if (net_run_command(cmd) != 0) {
		LOGE("file_extract_compressed_tar failed: %s -> %s",
		     archive_path, output_dir);
		return -1;
	}
	return 0;
}

int file_extract_tar(const char *archive_path, const char *output_dir)
{
	return file_extract_compressed_tar(archive_path, output_dir);
}

int file_copy(const char *src, const char *dst)
{
	char cmd[768];

	if (!src || !dst)
		return -1;
	if (file_ensure_dir(dst) != 0)
		return -1;
	snprintf(cmd, sizeof(cmd), "cp -a '%s' '%s'", src, dst);
	if (net_run_command(cmd) != 0) {
		LOGE("file_copy failed: %s -> %s", src, dst);
		return -1;
	}
	return 0;
}
