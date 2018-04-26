/* Copyright (c) 2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <fdserver.h>

#include "share_pipe_common.h"

int main(int argc, char *argv[])
{
	int fd;

	sleep(2); /* give time to register, etc */

	fd = fdserver_lookup_fd(FD_SRV_CTX_ISHM, SHARE_PIPE_KEY_WRITER);
	if (fd == -1) {
		fprintf(stderr, "Could not retrive fd\n");
		exit(EXIT_FAILURE);
	}

	printf("Writer: got file descriptor %d, sending\n", fd);
	write(fd, &fd, sizeof(int));
	printf("Writer:  done\n");
	close(fd);

	exit(EXIT_SUCCESS);
}
