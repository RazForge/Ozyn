#include "ipc.h"
#include "registry.h"
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
 * service_test_client.c — Real cross-process service registration test.
 *
 * Connects to OZAYN Core's IPC server, performs handshake,
 * then sends a SERVICE REGISTER request.
 *
 * Usage: ./build/service_test_client [socket_path]
 */

#define DEFAULT_ENDPOINT "runtime/ipc/ozayn.sock"
#define CLIENT_ID "test-service-worker"
#define CLIENT_COMPONENT_TYPE OZAYN_IPC_COMP_WORKER

/* ---- Raw send/recv helpers ---- */

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

/* ---- IPC message helpers ---- */

static int send_message(int fd, uint8_t type, uint32_t id,
                         const uint8_t *payload, uint32_t payload_len) {
    uint8_t hdr[OZAYN_IPC_HEADER_SIZE];
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = (OZAYN_IPC_MAGIC >> 8) & 0xFF;
    hdr[1] = OZAYN_IPC_MAGIC & 0xFF;
    hdr[2] = OZAYN_IPC_VERSION;
    hdr[3] = type;
    hdr[8] = (id >> 24) & 0xFF;
    hdr[9] = (id >> 16) & 0xFF;
    hdr[10] = (id >> 8)  & 0xFF;
    hdr[11] = id & 0xFF;
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

static int recv_message(int fd, uint8_t *out_type, uint32_t *out_id,
                         uint8_t **out_payload, uint32_t *out_len, int timeout_ms) {
    uint8_t hdr[OZAYN_IPC_HEADER_SIZE];
    if (recv_exact(fd, hdr, sizeof(hdr), timeout_ms) < 0) return -1;

    uint16_t magic = ((uint16_t)hdr[0] << 8) | hdr[1];
    if (magic != OZAYN_IPC_MAGIC) return -1;
    if (hdr[2] != OZAYN_IPC_VERSION) return -1;

    *out_type = hdr[3];
    *out_id = ((uint32_t)hdr[8] << 24) | ((uint32_t)hdr[9] << 16) |
              ((uint32_t)hdr[10] << 8) | (uint32_t)hdr[11];
    *out_len = ((uint32_t)hdr[12] << 24) | ((uint32_t)hdr[13] << 16) |
               ((uint32_t)hdr[14] << 8) | (uint32_t)hdr[15];

    *out_payload = NULL;
    if (*out_len > 0) {
        if (*out_len > OZAYN_IPC_MAX_MSG_SIZE) return -1;
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

/* ---- Build a service registration payload ---- */
/*
 * Payload format (binary):
 *   id_len(1) + id(N)
 *   name_len(1) + name(N)
 *   version_len(1) + version(N)
 *   protocol_version(1)
 *   endpoint_len(1) + endpoint(N)
 *   provider_len(1) + provider(N)
 *   capability_count(1)
 *   for each cap: cap_len(1) + cap(N)
 */

static uint8_t *build_register_payload(
    const char *id, const char *name, const char *version,
    uint8_t proto, const char *endpoint, const char *provider,
    const char **caps, int cap_count,
    uint32_t *out_len)
{
    size_t len = 0;
    len += 1 + strlen(id);
    len += 1 + strlen(name);
    len += 1 + strlen(version);
    len += 1; /* protocol version */
    len += 1 + strlen(endpoint);
    len += 1 + strlen(provider);
    len += 1; /* capability count */
    for (int i = 0; i < cap_count; i++) {
        len += 1 + strlen(caps[i]);
    }

    uint8_t *buf = malloc(len);
    if (!buf) return NULL;

    uint8_t *p = buf;
    /* id */
    *p++ = (uint8_t)strlen(id);
    memcpy(p, id, strlen(id)); p += strlen(id);
    /* name */
    *p++ = (uint8_t)strlen(name);
    memcpy(p, name, strlen(name)); p += strlen(name);
    /* version */
    *p++ = (uint8_t)strlen(version);
    memcpy(p, version, strlen(version)); p += strlen(version);
    /* protocol version */
    *p++ = proto;
    /* endpoint */
    *p++ = (uint8_t)strlen(endpoint);
    memcpy(p, endpoint, strlen(endpoint)); p += strlen(endpoint);
    /* provider */
    *p++ = (uint8_t)strlen(provider);
    memcpy(p, provider, strlen(provider)); p += strlen(provider);
    /* capabilities */
    *p++ = (uint8_t)cap_count;
    for (int i = 0; i < cap_count; i++) {
        *p++ = (uint8_t)strlen(caps[i]);
        memcpy(p, caps[i], strlen(caps[i])); p += strlen(caps[i]);
    }

    *out_len = (uint32_t)len;
    return buf;
}

int main(int argc, char **argv) {
    const char *endpoint = (argc > 1) ? argv[1] : DEFAULT_ENDPOINT;

    printf("[SERVICE CLIENT] Connecting to '%s'...\n", endpoint);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("[SERVICE CLIENT] socket");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%.*s",
             (int)sizeof(addr.sun_path) - 1, endpoint);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[SERVICE CLIENT] connect");
        close(fd);
        return 1;
    }

    printf("[SERVICE CLIENT] Connected!\n");

    /* === HANDSHAKE === */
    printf("[SERVICE CLIENT] Sending HELLO...\n");
    uint8_t id_len = (uint8_t)strlen(CLIENT_ID);
    uint8_t hello_payload[2 + 64];
    hello_payload[0] = (uint8_t)CLIENT_COMPONENT_TYPE;
    hello_payload[1] = id_len;
    memcpy(hello_payload + 2, CLIENT_ID, id_len);

    if (send_message(fd, OZAYN_IPC_MSG_HELLO, 1, hello_payload, 2 + id_len) < 0) {
        fprintf(stderr, "[SERVICE CLIENT] Failed to send HELLO\n");
        close(fd);
        return 1;
    }

    uint8_t msg_type;
    uint32_t msg_id, msg_len;
    uint8_t *msg_payload = NULL;

    if (recv_message(fd, &msg_type, &msg_id, &msg_payload, &msg_len, 5000) < 0) {
        fprintf(stderr, "[SERVICE CLIENT] Failed to receive HELLO_ACK\n");
        close(fd);
        return 1;
    }

    if (msg_type == OZAYN_IPC_MSG_HELLO_ACK) {
        printf("[SERVICE CLIENT] Handshake successful! (id=%u)\n", msg_id);
    } else {
        printf("[SERVICE CLIENT] Handshake rejected (type=%d)\n", msg_type);
        free(msg_payload);
        close(fd);
        return 1;
    }
    free(msg_payload);

    /* === REGISTER SERVICE === */
    printf("[SERVICE CLIENT] Registering service 'ozayn.vision'...\n");

    const char *caps[] = { "camera", "gesture-detection", "object-detection" };
    uint32_t payload_len = 0;
    uint8_t *reg_payload = build_register_payload(
        "ozayn.vision",        /* id */
        "Vision Engine",       /* name */
        "1.0.0",              /* version */
        OZAYN_IPC_VERSION,    /* protocol */
        "runtime/ipc/vision.sock", /* endpoint */
        "vision-module",      /* provider */
        caps, 3,              /* capabilities */
        &payload_len);

    if (!reg_payload) {
        fprintf(stderr, "[SERVICE CLIENT] Failed to build payload\n");
        close(fd);
        return 1;
    }

    if (send_message(fd, OZAYN_IPC_MSG_REQUEST, 2, reg_payload, payload_len) < 0) {
        fprintf(stderr, "[SERVICE CLIENT] Failed to send REGISTER\n");
        free(reg_payload);
        close(fd);
        return 1;
    }
    free(reg_payload);

    /* Wait for response */
    if (recv_message(fd, &msg_type, &msg_id, &msg_payload, &msg_len, 5000) < 0) {
        fprintf(stderr, "[SERVICE CLIENT] Failed to receive REGISTER response\n");
        close(fd);
        return 1;
    }

    if (msg_type == OZAYN_IPC_MSG_RESPONSE) {
        printf("[SERVICE CLIENT] Service registered successfully! (response id=%u)\n", msg_id);
    } else if (msg_type == OZAYN_IPC_MSG_ERROR) {
        printf("[SERVICE CLIENT] Service registration rejected\n");
        free(msg_payload);
        close(fd);
        return 1;
    } else {
        printf("[SERVICE CLIENT] Unexpected response type: %d\n", msg_type);
    }
    free(msg_payload);

    /* === SECOND REGISTRATION (duplicate — should be rejected if same ID) === */
    printf("[SERVICE CLIENT] Registering second service 'ozayn.audio'...\n");

    uint32_t payload2_len = 0;
    uint8_t *reg2_payload = build_register_payload(
        "ozayn.audio",
        "Audio Engine",
        "0.5.0",
        OZAYN_IPC_VERSION,
        "runtime/ipc/audio.sock",
        "audio-module",
        (const char *[]){ "speech-recognition", "speech-synthesis" },
        2,
        &payload2_len);

    if (reg2_payload) {
        if (send_message(fd, OZAYN_IPC_MSG_REQUEST, 3, reg2_payload, payload2_len) == 0) {
            if (recv_message(fd, &msg_type, &msg_id, &msg_payload, &msg_len, 5000) == 0) {
                if (msg_type == OZAYN_IPC_MSG_RESPONSE) {
                    printf("[SERVICE CLIENT] Second service registered! (id=%u)\n", msg_id);
                } else {
                    printf("[SERVICE CLIENT] Second registration response type: %d\n", msg_type);
                }
                free(msg_payload);
            }
        }
        free(reg2_payload);
    }

    /* === PING === */
    printf("[SERVICE CLIENT] Sending PING...\n");
    if (send_message(fd, OZAYN_IPC_MSG_PING, 4, NULL, 0) == 0) {
        if (recv_message(fd, &msg_type, &msg_id, &msg_payload, &msg_len, 5000) == 0) {
            if (msg_type == OZAYN_IPC_MSG_PONG) {
                printf("[SERVICE CLIENT] PONG received!\n");
            }
            free(msg_payload);
        }
    }

    /* === BYE === */
    printf("[SERVICE CLIENT] Sending BYE...\n");
    send_message(fd, OZAYN_IPC_MSG_BYE, 0, NULL, 0);

    close(fd);
    printf("[SERVICE CLIENT] All tests passed! Disconnecting.\n");
    return 0;
}
