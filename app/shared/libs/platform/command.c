#include "platform/command.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

int net_run_command(const char *command)
{
	int rc;

	if (!command || command[0] == '\0')
		return -1;

	rc = system(command);
	if (rc == -1)
		return -1;
	if (WIFEXITED(rc))
		return WEXITSTATUS(rc);

	return -1;
}

int net_read_command_output(const char *command, char *buffer,
			      size_t buffer_size)
{
	FILE *fp;
	size_t used = 0;

	if (!command || !buffer || buffer_size == 0)
		return -1;

	buffer[0] = '\0';
	fp = popen(command, "r");
	if (!fp)
		return -1;

	while (used + 1 < buffer_size) {
		size_t n = fread(buffer + used, 1, buffer_size - used - 1, fp);
		used += n;
		if (n == 0)
			break;
	}

	buffer[used] = '\0';
	return pclose(fp);
}
