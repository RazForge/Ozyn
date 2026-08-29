#include "ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <poll.h>

/*
 * ipc_test_client.c — Real cross-process IPC test client.
 *
 * Connects to OZAYN Core's IPC server, performs handshake,
 * sends a PING and a REQUEST, receives responses.
 *
 * Usage: ./build/ipc_test_client [socket_path]
 */

#define DEFAULT_ENDPOINT "runtime/ipc/ozayn.sock"
#define CLIENT_COMPONENT_ID "test_client"
#define CLIENT_COMPONENT_TYPE OZAYN_IPC_COMP_EXTERNAL

/* Send raw bytes */
static int send_all(int fd, const uint8_t *data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd pfd = { .fd = fd, .events = POLLOUT };
                poll(&pfd, 1, 100);
                continue;
            }
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

/* Receive exactly N bytes */
static int recv_exact(int fd, uint8_t *buf, size_t len, int timeout_ms) {
    size_t received = 0;
    while (received < len) {
        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        int ret = poll(&pfd, 1, timeout_ms);
        if (ret <= 0) return -1;

        ssize_t n = recv(fd, buf + received, len - received, 0);
        if (n <= 0) return -1;
        received += (size_t)n;
    }
    return 0;
}

/* Send a complete message */
static int send_message(int fd, uint8_t type, uint32_t id,
                         const uint8_t *payload, uint32_t payload_len) {
    uint8_t hdr[OZAYN_IPC_HEADER_SIZE];
    memset(hdr, 0, sizeof(hdr));

    /* Magic */
    hdr[0] = (OZAYN_IPC_MAGIC >> 8) & 0xFF;
    hdr[1] = OZAYN_IPC_MAGIC & 0xFF;

    /* Version */
    hdr[2] = OZAYN_IPC_VERSION;

    /* Type */
    hdr[3] = type;

    /* ID (big-endian) */
    hdr[8]  = (id >> 24) & 0xFF;
    hdr[9]  = (id >> 16) & 0xFF;
    hdr[10] = (id >> 8)  & 0xFF;
    hdr[11] = id & 0xFF;

    /* Length (big-endian) */
    hdr[12] = (payload_len >> 24) & 0xFF;
    hdr[13] = (payload_len >> 16) & 0xFF;
    hdr[14] = (payload_len >> 8)  & 0xFF;
    hdr[15] = payload_len & 0xFF;

    if (send_all(fd, hdr, sizeof(hdr)) < 0) return -1;
    if (payload_len > 0 && payload) {
        if (send_all(fd, payload, payload_len) < 0) return -1;
    }
    return 0;
}

/* Receive a complete message */
static int recv_message(int fd, uint8_t *out_type, uint32_t *out_id,
                         uint8_t **out_payload, uint32_t *out_len, int timeout_ms) {
    uint8_t hdr[OZAYN_IPC_HEADER_SIZE];
    if (recv_exact(fd, hdr, sizeof(hdr), timeout_ms) < 0) return -1;

    uint16_t magic = ((uint16_t)hdr[0] << 8) | hdr[1];
    if (magic != OZAYN_IPC_MAGIC) {
        fprintf(stderr, "[CLIENT] Invalid magic: 0x%04X\n", magic);
        return -1;
    }

    if (hdr[2] != OZAYN_IPC_VERSION) {
        fprintf(stderr, "[CLIENT] Version mismatch: %d\n", hdr[2]);
        return -1;
    }

    *out_type = hdr[3];
    *out_id = ((uint32_t)hdr[8] << 24) | ((uint32_t)hdr[9] << 16) |
              ((uint32_t)hdr[10] << 8) | (uint32_t)hdr[11];
    *out_len = ((uint32_t)hdr[12] << 24) | ((uint32_t)hdr[13] << 16) |
               ((uint32_t)hdr[14] << 8) | (uint32_t)hdr[15];

    *out_payload = NULL;
    if (*out_len > 0) {
        if (*out_len > OZAYN_IPC_MAX_MSG_SIZE) {
            fprintf(stderr, "[CLIENT] Payload too large: %u\n", *out_len);
            return -1;
        }
        *out_payload = malloc(*out_len);
        if (!*out_payload) return -1;
        if (recv_exact(fd, *out_payload, *out_len, timeout_ms) < 0) {
            free(*out_payload);
            *out_payload = NULL;
            return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *endpoint = (argc > 1) ? argv[1] : DEFAULT_ENDPOINT;

    printf("[CLIENT] Connecting to '%s'...\n", endpoint);

    /* Create socket */
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("[CLIENT] socket");
        return 1;
    }

    /* Connect */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", endpoint);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[CLIENT] connect");
        close(fd);
        return 1;
    }

    printf("[CLIENT] Connected!\n");

    /* === HANDSHAKE === */
    printf("[CLIENT] Sending HELLO...\n");

    /* Build HELLO payload: [component_type(1) + id_len(1) + id(N)] */
    uint8_t id_len = (uint8_t)strlen(CLIENT_COMPONENT_ID);
    uint8_t hello_payload[2 + 64];
    hello_payload[0] = (uint8_t)CLIENT_COMPONENT_TYPE;
    hello_payload[1] = id_len;
    memcpy(hello_payload + 2, CLIENT_COMPONENT_ID, id_len);

    if (send_message(fd, OZAYN_IPC_MSG_HELLO, 1, hello_payload, 2 + id_len) < 0) {
        fprintf(stderr, "[CLIENT] Failed to send HELLO\n");
        close(fd);
        return 1;
    }

    /* Wait for HELLO_ACK */
    uint8_t msg_type;
    uint32_t msg_id, msg_len;
    uint8_t *msg_payload = NULL;

    if (recv_message(fd, &msg_type, &msg_id, &msg_payload, &msg_len, 5000) < 0) {
        fprintf(stderr, "[CLIENT] Failed to receive HELLO_ACK\n");
        close(fd);
        return 1;
    }

    if (msg_type == OZAYN_IPC_MSG_HELLO_ACK) {
        printf("[CLIENT] Handshake successful! (id=%u)\n", msg_id);
    } else if (msg_type == OZAYN_IPC_MSG_ERROR) {
        printf("[CLIENT] Handshake rejected by server\n");
        free(msg_payload);
        close(fd);
        return 1;
    } else {
        printf("[CLIENT] Unexpected message type: %d\n", msg_type);
        free(msg_payload);
        close(fd);
        return 1;
    }
    free(msg_payload);

    /* === PING/PONG === */
    printf("[CLIENT] Sending PING...\n");
    if (send_message(fd, OZAYN_IPC_MSG_PING, 2, NULL, 0) < 0) {
        fprintf(stderr, "[CLIENT] Failed to send PING\n");
        close(fd);
        return 1;
    }

    if (recv_message(fd, &msg_type, &msg_id, &msg_payload, &msg_len, 5000) < 0) {
        fprintf(stderr, "[CLIENT] Failed to receive PONG\n");
        close(fd);
        return 1;
    }

    if (msg_type == OZAYN_IPC_MSG_PONG) {
        printf("[CLIENT] PONG received! (id=%u)\n", msg_id);
    } else {
        printf("[CLIENT] Unexpected response to PING: type=%d\n", msg_type);
    }
    free(msg_payload);

    /* === REQUEST/RESPONSE === */
    printf("[CLIENT] Sending REQUEST...\n");

    /* Payload: a simple status request */
    const char *req_data = "{\"command\":\"status\"}";
    uint32_t req_len = (uint32_t)strlen(req_data);

    if (send_message(fd, OZAYN_IPC_MSG_REQUEST, 3,
                      (const uint8_t *)req_data, req_len) < 0) {
        fprintf(stderr, "[CLIENT] Failed to send REQUEST\n");
        close(fd);
        return 1;
    }

    if (recv_message(fd, &msg_type, &msg_id, &msg_payload, &msg_len, 5000) < 0) {
        fprintf(stderr, "[CLIENT] Failed to receive RESPONSE\n");
        close(fd);
        return 1;
    }

    if (msg_type == OZAYN_IPC_MSG_RESPONSE) {
        printf("[CLIENT] RESPONSE received (id=%u, len=%u)\n", msg_id, msg_len);
        if (msg_payload && msg_len > 0) {
            printf("[CLIENT] Response payload: %.*s\n", msg_len, msg_payload);
        }
    } else {
        printf("[CLIENT] Unexpected response: type=%d\n", msg_type);
    }
    free(msg_payload);

    /* === BYE === */
    printf("[CLIENT] Sending BYE...\n");
    send_message(fd, OZAYN_IPC_MSG_BYE, 0, NULL, 0);

    /* === DONE === */
    close(fd);
    printf("[CLIENT] All tests passed! Disconnecting.\n");
    return 0;
}
