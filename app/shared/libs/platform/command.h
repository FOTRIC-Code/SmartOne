#ifndef NET_APP_PLATFORM_COMMAND_H
#define NET_APP_PLATFORM_COMMAND_H

#include <stddef.h>

int net_run_command(const char *command);
int net_read_command_output(const char *command, char *buffer,
			      size_t buffer_size);

#endif
