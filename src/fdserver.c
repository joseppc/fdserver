/* Copyright (c) 2016-2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

#include <unistd.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include <fdserver.h>
#include <odp_adapt.h>
#include <fdserver_internal.h>

#define FDSERVER_BACKLOG 5
/* define the tables of file descriptors handled by this server: */
#define FDSERVER_MAX_ENTRIES 256
typedef struct fdentry_s {
	fdserver_context_e context;
	uint64_t key;
	int  fd;
} fdentry_t;
static fdentry_t *fd_table;
static int fd_table_nb_entries;

/*
 * server function
 * receive a client request and handle it.
 * Always returns 0 unless a stop request is received.
 */
static int handle_request(int client_sock)
{
	int command;
	fdserver_context_e context;
	uint64_t key;
	int fd;
	int i;

	/* get a client request: */
	fdserver_internal_recv_msg(client_sock, &command, &context, &key, &fd);
	switch (command) {
	case FD_REGISTER_REQ:
		if ((fd < 0) || (context >= FD_SRV_CTX_END)) {
			ODP_ERR("Invalid register fd or context\n");
			fdserver_internal_send_msg(client_sock,
						   FD_RETVAL_FAILURE,
						   FD_SRV_CTX_NA, 0, -1);
			return 0;
		}

		/* store the file descriptor in table: */
		if (fd_table_nb_entries < FDSERVER_MAX_ENTRIES) {
			fd_table[fd_table_nb_entries].context = context;
			fd_table[fd_table_nb_entries].key     = key;
			fd_table[fd_table_nb_entries++].fd    = fd;
			FD_ODP_DBG("storing {ctx=%d, key=%" PRIu64 "}->fd=%d\n",
				   context, key, fd);
		} else {
			ODP_ERR("FD table full\n");
			fdserver_internal_send_msg(client_sock,
						   FD_RETVAL_FAILURE,
						   FD_SRV_CTX_NA, 0, -1);
			return 0;
		}

		fdserver_internal_send_msg(client_sock, FD_RETVAL_SUCCESS,
					   FD_SRV_CTX_NA, 0, -1);
		break;

	case FD_LOOKUP_REQ:
		if (context >= FD_SRV_CTX_END) {
			ODP_ERR("invalid lookup context\n");
			fdserver_internal_send_msg(client_sock,
						   FD_RETVAL_FAILURE,
						   FD_SRV_CTX_NA, 0, -1);
			return 0;
		}

		/* search key in table and sent reply: */
		for (i = 0; i < fd_table_nb_entries; i++) {
			if ((fd_table[i].context == context) &&
			    (fd_table[i].key == key)) {
				fd = fd_table[i].fd;
				ODP_DBG("lookup {ctx=%d,"
					" key=%" PRIu64 "}->fd=%d\n",
					context, key, fd);
				fdserver_internal_send_msg(client_sock,
							   FD_RETVAL_SUCCESS,
							   context, key,
							   fd);
				return 0;
			}
		}

		/* context+key not found... send nack */
		fdserver_internal_send_msg(client_sock, FD_RETVAL_FAILURE,
					   context, key, -1);
		break;

	case FD_DEREGISTER_REQ:
		if (context >= FD_SRV_CTX_END) {
			ODP_ERR("invalid deregister context\n");
			fdserver_internal_send_msg(client_sock,
						   FD_RETVAL_FAILURE,
						   FD_SRV_CTX_NA, 0, -1);
			return 0;
		}

		/* search key in table and remove it if found, and reply: */
		for (i = 0; i < fd_table_nb_entries; i++) {
			if ((fd_table[i].context == context) &&
			    (fd_table[i].key == key)) {
				FD_ODP_DBG("drop {ctx=%d,"
					   " key=%" PRIu64 "}->fd=%d\n",
					   context, key, fd_table[i].fd);
				close(fd_table[i].fd);
				fd_table[i] = fd_table[--fd_table_nb_entries];
				fdserver_internal_send_msg(client_sock,
							   FD_RETVAL_SUCCESS,
							   context, key, -1);
				return 0;
			}
		}

		/* key not found... send nack */
		fdserver_internal_send_msg(client_sock, FD_RETVAL_FAILURE,
					   context, key, -1);
		break;

	case FD_SERVERSTOP_REQ:
		FD_ODP_DBG("Stoping FD server\n");
		return 1;

	default:
		ODP_ERR("Unexpected request\n");
		break;
	}
	return 0;
}

/*
 * server function
 * loop forever, handling client requests one by one
 */
static void wait_requests(int sock)
{
	int c_socket; /* client connection */
	unsigned int addr_sz;
	struct sockaddr_un remote;

	for (;;) {
		addr_sz = sizeof(remote);
		c_socket = accept(sock, (struct sockaddr *)&remote, &addr_sz);
		if (c_socket == -1) {
			if (errno == EINTR)
				continue;

			ODP_ERR("wait_requests: %s\n", strerror(errno));
			return;
		}

		if (handle_request(c_socket))
			break;
		close(c_socket);
	}
	close(c_socket);
}

/*
 * Create a unix domain socket and fork a process to listen to incoming
 * requests.
 */
int _odp_fdserver_init_global(void)
{
	const char *sockpath = fdserver_path;
	int sock;
	struct sockaddr_un local;
	pid_t server_pid;
	int res;

	/* create UNIX domain socket: */
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1) {
		ODP_ERR("_odp_fdserver_init_global: %s\n", strerror(errno));
		return -1;
	}

	/* remove previous named socket if it already exists: */
	unlink(sockpath);

	/* bind to new named socket: */
	local.sun_family = AF_UNIX;
	strncpy(local.sun_path, sockpath, sizeof(local.sun_path));
	res = bind(sock, (struct sockaddr *)&local, sizeof(struct sockaddr_un));
	if (res == -1) {
		ODP_ERR("_odp_fdserver_init_global: %s\n", strerror(errno));
		close(sock);
		return -1;
	}

	/* listen for incoming conections: */
	if (listen(sock, FDSERVER_BACKLOG) == -1) {
		ODP_ERR("_odp_fdserver_init_global: %s\n", strerror(errno));
		close(sock);
		return -1;
	}

	/* allocate the space for the file descriptor<->key table: */
	fd_table = malloc(FDSERVER_MAX_ENTRIES * sizeof(fdentry_t));
	if (!fd_table) {
		ODP_ERR("maloc failed!\n");
		exit(1);
	}

	/* wait for clients requests */
	wait_requests(sock); /* Returns when server is stopped  */
	close(sock);

	/* release the file descriptor table: */
	free(fd_table);

	return 0;
}

int main(int argc, char *argv[])
{
	if (_odp_fdserver_init_global() != 0)
		exit(EXIT_FAILURE);
	exit(EXIT_SUCCESS);
}
