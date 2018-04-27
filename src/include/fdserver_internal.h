/* Copyright (c) 2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
#ifndef FDSERVER_INTERNAL
#define FDSERVER_INTERNAL

#include <stdint.h>

#define FDSERVER_BACKLOG 5

#define FD_ODP_DEBUG_PRINT 0

#define FD_ODP_DBG(fmt, ...) \
	do { \
		if (FD_ODP_DEBUG_PRINT == 1) \
			ODP_DBG(fmt, ##__VA_ARGS__);\
	} while (0)

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
 * define the message struct used for communication between client and server
 * (this single message is used in both direction)
 * The file descriptors are sent out of band as ancillary data for conversion.
 */
typedef struct fd_server_msg {
	int command;
	fdserver_context_e context;
	uint64_t key;
} fdserver_msg_t;
/* possible commands are: */
#define FD_REGISTER_REQ		1  /* client -> server */
#define FD_REGISTER_ACK		2  /* server -> client */
#define FD_REGISTER_NACK	3  /* server -> client */
#define FD_LOOKUP_REQ		4  /* client -> server */
#define FD_LOOKUP_ACK		5  /* server -> client */
#define FD_LOOKUP_NACK		6  /* server -> client */
#define FD_DEREGISTER_REQ	7  /* client -> server */
#define FD_DEREGISTER_ACK	8  /* server -> client */
#define FD_DEREGISTER_NACK	9  /* server -> client */
#define FD_SERVERSTOP_REQ	10 /* client -> server (stops) */

int fdserver_internal_send_msg(int sock, int command,
			       fdserver_context_e context,
			       uint64_t key, int fd_to_send);

int fdserver_internal_recv_msg(int sock, int *command,
			       fdserver_context_e *context,
			       uint64_t *key, int *fd_to_send);

#endif
