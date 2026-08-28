/**
 * Ozayn IPC — Unix Domain Socket binary protocol.
 * Fastest local IPC. No serialization overhead.
 */

#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

int ipc_server_init(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, OZAYN_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    unlink(OZAYN_SOCKET_PATH);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, 1) < 0) {
        close(fd);
        return -1;
    }

    printf("[IPC] Server listening on %s\n", OZAYN_SOCKET_PATH);
    return fd;
}

int ipc_client_init(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, OZAYN_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    int retries = 50;
    while (retries > 0) {
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
            return fd;
        usleep(100000); /* 100ms */
        retries--;
    }

    close(fd);
    return -1;
}

int ipc_send(int fd, const ozayn_msg_t *msg)
{
    int off = 0;
    uint8_t buf[8 + 4096];

    buf[off++] = msg->type;
    buf[off++] = msg->code;
    buf[off++] = (msg->length >> 8) & 0xFF;
    buf[off++] = msg->length & 0xFF;
    buf[off++] = (msg->seq >> 24) & 0xFF;
    buf[off++] = (msg->seq >> 16) & 0xFF;
    buf[off++] = (msg->seq >> 8)  & 0xFF;
    buf[off++] = msg->seq & 0xFF;

    if (msg->length > 0 && msg->length <= 4096) {
        memcpy(buf + off, msg->data, msg->length);
        off += msg->length;
    }

    ssize_t sent = write(fd, buf, off);
    return (sent == off) ? 0 : -1;
}

int ipc_recv(int fd, ozayn_msg_t *msg)
{
    uint8_t header[8];
    ssize_t n = read(fd, header, 8);
    if (n != 8) return -1;

    msg->type   = header[0];
    msg->code   = header[1];
    msg->length = (header[2] << 8) | header[3];
    msg->seq    = (header[4] << 24) | (header[5] << 16) |
                  (header[6] << 8)  | header[7];

    if (msg->length > 0 && msg->length <= 4096) {
        ssize_t got = read(fd, msg->data, msg->length);
        if (got != msg->length) return -1;
    }

    return 0;
}

void ipc_close(int fd)
{
    if (fd >= 0) close(fd);
}
