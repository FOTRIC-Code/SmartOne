#ifndef NET_APP_LIBS_FILE_UTIL_H
#define NET_APP_LIBS_FILE_UTIL_H

#include <stddef.h>
#include <sys/types.h>

void file_mkdir_p(const char *path);
int file_exists(const char *path);
int file_ensure_dir(const char *path);
int create_dir_recursive(const char *path);
int file_write(const char *path, const char *data);
int file_read(const char *path, char **out_buf, size_t *out_len);
int file_remove_tree(const char *path);
int file_remove_tree_contents(const char *path);
/* Remove file or directory tree; missing path is success; logs on failure. */
void file_remove_path_best_effort(const char *path);
int file_chmod(const char *path, mode_t mode);
long file_size(const char *path);
int file_extract_compressed_tar(const char *archive_path,
				const char *output_dir);

/* Plain or gzip tar extract (recipe packages). */
int file_extract_tar(const char *archive_path, const char *output_dir);

int file_copy(const char *src, const char *dst);

#endif
