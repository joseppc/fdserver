/* Copyright (c) 2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include <fdserver.h>

#include "share_pipe_common.h"

int main(int argc, char *argv[])
{
	int fd[2];
	int ret;
	int data;

	ret = pipe(fd);
	if (ret == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}

	ret = fdserver_register_fd(FD_SRV_CTX_ISHM,
				   SHARE_PIPE_KEY_WRITER,
				   fd[1]);
	if (ret == -1) {
		fprintf(stderr, "failed to register fd\n");
		exit(EXIT_FAILURE);
	}

	close(fd[1]);

	/* wait for the other end to write an integer */
	while (read(fd[0], &data, sizeof(int)) == -1) {
		if (errno == EAGAIN || errno == EINTR) {
			printf("again\n");
			continue;
		}
		perror("read");
		exit(EXIT_FAILURE);
	}

	printf("Reader: Received: %d\n", data);

	close(fd[0]);

	fdserver_terminate(FD_SRV_CTX_ISHM);

	exit(EXIT_SUCCESS);
}

