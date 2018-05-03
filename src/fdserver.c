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
struct fdentry {
	uint64_t key;
	int  fd;
};

struct fdcontext_entry {
	fdserver_context_e context;
	int max_entries;
	int num_entries;
	struct fdentry fd_table[0];
};
static struct fdcontext_entry *context_table;
static int fdentry_table_count = 0;

static void handle_new_context(int client_sock)
{
	size_t size;

	if (context_table != NULL)
		goto send_error;

	size = sizeof(struct fdcontext_entry) +
		FDSERVER_MAX_ENTRIES * sizeof(struct fdentry);

	context_table = malloc(size);
	if (context_table != NULL) {
		memset(context_table, 0, size);
		context_table->context = FD_SRV_CTX_ISHM;
		context_table->max_entries = FDSERVER_MAX_ENTRIES;
		context_table->num_entries = 0;
		fdserver_internal_send_msg(client_sock,
					   FD_RETVAL_SUCCESS,
					   FD_SRV_CTX_NA, 0, -1);
		FD_ODP_DBG("New context created\n");
		return;
	}

send_error:
	FD_ODP_DBG("Failed to create new context\n");
	fdserver_internal_send_msg(client_sock,
				   FD_RETVAL_FAILURE,
				   FD_SRV_CTX_NA, 0, -1);
}

static struct fdcontext_entry *find_context(fdserver_context_e *context)
{
	return context_table;
}

static int add_fdentry(struct fdcontext_entry *context, uint64_t key, int fd)
{
	struct fdentry *fdentry;

	if (context->num_entries >= context->max_entries)
		return -1;

	context->fd_table[context->num_entries].key = key;
	context->fd_table[context->num_entries].fd = fd;
	context->num_entries++;

	return 0;
}

static int find_fdentry_from_key(struct fdcontext_entry *context, uint64_t key)
{
	struct fdentry *fd_table;

	fd_table = &(context->fd_table[0]);
	for (int i = 0; i < context->num_entries; i++) {
		if (fd_table[i].key == key)
			return fd_table[i].fd;
	}

	return -1;
}

static int del_fdentry(struct fdcontext_entry *context, uint64_t key)
{
	struct fdentry *fd_table;

	fd_table = &context->fd_table[0];
	for (int i = 0; i < context->num_entries; i++) {
		if (fd_table[i].key == key) {
			close(fd_table[i].fd);
			fd_table[i] = fd_table[--context->num_entries];
			return 0;
		}
	}

	return -1;
}

/*
 * server function
 * receive a client request and handle it.
 * Always returns 0 unless a stop request is received.
 */
static int handle_request(int client_sock)
{
	int command;
	fdserver_context_e ctx;
	struct fdcontext_entry *context;
	uint64_t key;
	int fd;
	int i;

	/* get a client request: */
	fdserver_internal_recv_msg(client_sock, &command, &ctx, &key, &fd);
	switch (command) {
	case FD_REGISTER_REQ:
		context = find_context(&ctx);
		if ((fd < 0) || (context == NULL)) {
			ODP_ERR("Invalid register fd or context\n");
			fdserver_internal_send_msg(client_sock,
						   FD_RETVAL_FAILURE,
						   FD_SRV_CTX_NA, 0, -1);
			return 0;
		}

		if (add_fdentry(context, key, fd) == 0) {
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
		context = find_context(&ctx);
		if (context == NULL) {
			ODP_ERR("invalid lookup context\n");
			fdserver_internal_send_msg(client_sock,
						   FD_RETVAL_FAILURE,
						   FD_SRV_CTX_NA, 0, -1);
			return 0;
		}

		fd = find_fdentry_from_key(context, key);
		if (fd == -1)
			command = FD_RETVAL_FAILURE;
		else
			command = FD_RETVAL_SUCCESS;

		fdserver_internal_send_msg(client_sock, command,
					   ctx, key, fd);

		FD_ODP_DBG("lookup {ctx=%d, key=%" PRIu64 "}->fd=%d\n",
			   ctx, key, fd);
		break;

	case FD_DEREGISTER_REQ:
		FD_ODP_DBG("Delete {ctx: %d, key: %" PRIu64 "}\n", ctx, key);
		command = FD_RETVAL_FAILURE;
		context = find_context(&ctx);
		if (context != NULL) {
			if (del_fdentry(context, key) == 0) {
				ODP_DBG("deleted {ctx=%d, key=%" PRIu64 "}\n",
					ctx, key);
				command = FD_RETVAL_SUCCESS;
			} else {
				ODP_DBG("Failed to delete deleted {ctx=%d, "
					"key=%" PRIu64 "}\n",
					ctx, key);
			}
		}
		fdserver_internal_send_msg(client_sock, command, ctx, key, -1);
		break;

	case FD_SERVERSTOP_REQ:
		FD_ODP_DBG("Stoping FD server\n");
		return 1;

	case FD_NEW_CONTEXT:
		handle_new_context(client_sock);
		break;

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

	context_table = NULL;

	/* wait for clients requests */
	wait_requests(sock); /* Returns when server is stopped  */
	close(sock);

	/* release the file descriptor table: */
	free(context_table);

	return 0;
}

int main(int argc, char *argv[])
{
	if (_odp_fdserver_init_global() != 0)
		exit(EXIT_FAILURE);
	exit(EXIT_SUCCESS);
}
