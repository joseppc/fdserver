/* Copyright (c) 2016-2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */

/*
 * This file implements a file descriptor sharing server enabling
 * sharing of file descriptors between processes, regardless of fork time.
 *
 * File descriptors are process scoped, but they can be "sent and converted
 * on the fly" between processes using special unix domain socket ancillary
 * data.
 * The receiving process gets a file descriptor "pointing" to the same thing
 * as the one sent (but the value of the file descriptor itself may be different
 * from the one sent).
 * Because ODP applications are responsible for creating ODP threads (i.e.
 * pthreads or linux processes), ODP has no control on the order things happen:
 * Nothing prevent a thread A to fork B and C, and then C creating a pktio
 * which will be used by A and B to send/receive packets.
 * Assuming this pktio uses a file descriptor, the latter will need to be
 * shared between the processes, despite the "non convenient" fork time.
 * The shared memory allocator is likely to use this as well to be able to
 * share memory regardless of fork() time.
 * This server handles a table of {(context,key)<-> fd} pair, and is
 * interfaced by the following functions:
 *
 * _odp_fdserver_register_fd(context, key, fd_to_send);
 * _odp_fdserver_deregister_fd(context, key);
 * _odp_fdserver_lookup_fd(context, key);
 *
 * which are used to register/deregister or querry for file descriptor based
 * on a context and key value couple, which has to be unique.
 *
 * Note again that the file descriptors stored here are local to this server
 * process and get converted both when registered or looked up.
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

/* opens and returns a connected socket to the server */
static int get_socket(void)
{
	const char *sockpath = fdserver_path;
	int s_sock; /* server socket */
	struct sockaddr_un remote;
	int len;

	s_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s_sock == -1) {
		ODP_ERR("cannot connect to server: %s\n", strerror(errno));
		return -1;
	}

	remote.sun_family = AF_UNIX;
	strcpy(remote.sun_path, sockpath);
	len = strlen(remote.sun_path) + sizeof(remote.sun_family);
	while (connect(s_sock, (struct sockaddr *)&remote, len) == -1) {
		if (errno == EINTR)
			continue;
		ODP_ERR("cannot connect to server: %s\n", strerror(errno));
		close(s_sock);
		return -1;
	}

	return s_sock;
}

/*
 * Client function:
 * Register a file descriptor to the server. Return -1 on error.
 */
int _odp_fdserver_register_fd(fdserver_context_e context, uint64_t key,
			      int fd_to_send)
{
	int s_sock; /* server socket */
	int res;
	int command;
	int fd;

	FD_ODP_DBG("FD client register: pid=%d key=%" PRIu64 ", fd=%d\n",
		   getpid(), key, fd_to_send);

	s_sock = get_socket();
	if (s_sock < 0)
		return -1;

	res =  fdserver_internal_send_msg(s_sock, FD_REGISTER_REQ, context, key,
					  fd_to_send);
	if (res < 0) {
		ODP_ERR("fd registration failure\n");
		close(s_sock);
		return -1;
	}

	res = fdserver_internal_recv_msg(s_sock, &command, &context, &key, &fd);

	if ((res < 0) || (command != FD_REGISTER_ACK)) {
		ODP_ERR("fd registration failure\n");
		close(s_sock);
		return -1;
	}

	close(s_sock);

	return 0;
}

/*
 * Client function:
 * Deregister a file descriptor from the server. Return -1 on error.
 */
int _odp_fdserver_deregister_fd(fdserver_context_e context, uint64_t key)
{
	int s_sock; /* server socket */
	int res;
	int command;
	int fd;

	FD_ODP_DBG("FD client deregister: pid=%d key=%" PRIu64 "\n",
		   getpid(), key);

	s_sock = get_socket();
	if (s_sock < 0)
		return -1;

	res =  fdserver_internal_send_msg(s_sock, FD_DEREGISTER_REQ, context,
					  key, -1);
	if (res < 0) {
		ODP_ERR("fd de-registration failure\n");
		close(s_sock);
		return -1;
	}

	res = fdserver_internal_recv_msg(s_sock, &command, &context, &key, &fd);

	if ((res < 0) || (command != FD_DEREGISTER_ACK)) {
		ODP_ERR("fd de-registration failure\n");
		close(s_sock);
		return -1;
	}

	close(s_sock);

	return 0;
}

/*
 * client function:
 * lookup a file descriptor from the server. return -1 on error,
 * or the file descriptor on success (>=0).
 */
int _odp_fdserver_lookup_fd(fdserver_context_e context, uint64_t key)
{
	int s_sock; /* server socket */
	int res;
	int command;
	int fd;

	s_sock = get_socket();
	if (s_sock < 0)
		return -1;

	res =  fdserver_internal_send_msg(s_sock, FD_LOOKUP_REQ, context,
					  key, -1);
	if (res < 0) {
		ODP_ERR("fd lookup failure\n");
		close(s_sock);
		return -1;
	}

	res = fdserver_internal_recv_msg(s_sock, &command, &context, &key, &fd);

	if ((res < 0) || (command != FD_LOOKUP_ACK)) {
		ODP_ERR("fd lookup failure\n");
		close(s_sock);
		return -1;
	}

	close(s_sock);
	ODP_DBG("FD client lookup: pid=%d, key=%" PRIu64 ", fd=%d\n",
		getpid(), key, fd);

	return fd;
}

/*
 * request server terminaison:
 */
static int stop_server(void)
{
	int s_sock; /* server socket */
	int res;

	FD_ODP_DBG("FD sending server stop request\n");

	s_sock = get_socket();
	if (s_sock < 0)
		return -1;

	res =  fdserver_internal_send_msg(s_sock, FD_SERVERSTOP_REQ, 0, 0, -1);
	if (res < 0) {
		ODP_ERR("fd stop request failure\n");
		close(s_sock);
		return -1;
	}

	close(s_sock);

	return 0;
}

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
						   FD_REGISTER_NACK,
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
						   FD_REGISTER_NACK,
						   FD_SRV_CTX_NA, 0, -1);
			return 0;
		}

		fdserver_internal_send_msg(client_sock, FD_REGISTER_ACK,
					   FD_SRV_CTX_NA, 0, -1);
		break;

	case FD_LOOKUP_REQ:
		if (context >= FD_SRV_CTX_END) {
			ODP_ERR("invalid lookup context\n");
			fdserver_internal_send_msg(client_sock, FD_LOOKUP_NACK,
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
							   FD_LOOKUP_ACK,
							   context, key,
							   fd);
				return 0;
			}
		}

		/* context+key not found... send nack */
		fdserver_internal_send_msg(client_sock, FD_LOOKUP_NACK, context,
					   key, -1);
		break;

	case FD_DEREGISTER_REQ:
		if (context >= FD_SRV_CTX_END) {
			ODP_ERR("invalid deregister context\n");
			fdserver_internal_send_msg(client_sock,
						   FD_DEREGISTER_NACK,
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
							   FD_DEREGISTER_ACK,
							   context, key, -1);
				return 0;
			}
		}

		/* key not found... send nack */
		fdserver_internal_send_msg(client_sock, FD_DEREGISTER_NACK,
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

/*
 * Terminate the server
 */
int _odp_fdserver_term_global(void)
{
	int status;
	const char *sockpath = fdserver_path;

	/* close the server and wait for child terminaison*/
	stop_server();
	wait(&status);

	/* delete the UNIX domain socket: */
	unlink(sockpath);

	rmdir(sockpath);

	return 0;
}

int main(int argc, char *argv[])
{
	if (_odp_fdserver_init_global() != 0)
		exit(EXIT_FAILURE);
	exit(EXIT_SUCCESS);
}
