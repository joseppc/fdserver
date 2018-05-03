/* Copyright (c) 2018, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier:     BSD-3-Clause
 */
#ifndef FDSERVER_INTERNAL
#define FDSERVER_INTERNAL

#include <stdint.h>

#define FD_ODP_DEBUG_PRINT 0

#define FD_ODP_DBG(fmt, ...) \
	do { \
		if (FD_ODP_DEBUG_PRINT == 1) \
			ODP_DBG(fmt, ##__VA_ARGS__);\
	} while (0)

/*
 * define the message struct used for communication between client and server
 * (this single message is used in both direction)
 * The file descriptors are sent out of band as ancillary data for conversion.
 */
typedef struct fd_server_msg {
	union {
		int command;
		int retval;
	};
	fdserver_context_e context;
	uint64_t key;
} fdserver_msg_t;
/* possible commands are: */
#define FD_REGISTER_REQ		1 /* client -> server */
#define FD_LOOKUP_REQ		2 /* client -> server */
#define FD_DEREGISTER_REQ	3 /* client -> server */
#define FD_SERVERSTOP_REQ	4 /* client -> server (stops) */

/* possible return values from the server */
#define FD_RETVAL_SUCCESS	0
#define FD_RETVAL_FAILURE	1

int fdserver_internal_send_msg(int sock, int command,
			       fdserver_context_e context,
			       uint64_t key, int fd_to_send);

int fdserver_internal_recv_msg(int sock, int *command,
			       fdserver_context_e *context,
			       uint64_t *key, int *fd_to_send);

#endif
