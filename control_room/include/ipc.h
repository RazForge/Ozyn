/**
 * Ozayn IPC — Unix Domain Socket communication.
 * Fastest local IPC. Binary protocol. No JSON overhead.
 */

#ifndef OZAYN_IPC_H
#define OZAYN_IPC_H

#include "ozayn_core.h"

#define OZAYN_SOCKET_PATH "/tmp/ozayn_control.sock"

/* Initialize IPC server (control room side) */
int  ipc_server_init(void);

/* Initialize IPC client (ozayn system side) */
int  ipc_client_init(void);

/* Send message */
int  ipc_send(int fd, const ozayn_msg_t *msg);

/* Receive message (blocking) */
int  ipc_recv(int fd, ozayn_msg_t *msg);

/* Close connection */
void ipc_close(int fd);

#endif /* OZAYN_IPC_H */
