#include "ipc.h"
#include "logger.h"
#include "recovery.h"
#include "security.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <time.h>

/* ================================================================
 * NAMES
 * ================================================================ */

const char *ozayn_ipc_msg_type_name(ozayn_ipc_msg_type_t type) {
    switch (type) {
        case OZAYN_IPC_MSG_NONE:      return "NONE";
        case OZAYN_IPC_MSG_HELLO:     return "HELLO";
        case OZAYN_IPC_MSG_HELLO_ACK: return "HELLO_ACK";
        case OZAYN_IPC_MSG_REQUEST:   return "REQUEST";
        case OZAYN_IPC_MSG_RESPONSE:  return "RESPONSE";
        case OZAYN_IPC_MSG_EVENT:     return "EVENT";
        case OZAYN_IPC_MSG_ERROR:     return "ERROR";
        case OZAYN_IPC_MSG_PING:      return "PING";
        case OZAYN_IPC_MSG_PONG:      return "PONG";
        case OZAYN_IPC_MSG_BYE:       return "BYE";
    }
    return "UNKNOWN";
}

const char *ozayn_ipc_conn_state_name(ozayn_ipc_conn_state_t state) {
    switch (state) {
        case OZAYN_IPC_CONN_DISCONNECTED: return "DISCONNECTED";
        case OZAYN_IPC_CONN_CONNECTING:   return "CONNECTING";
        case OZAYN_IPC_CONN_HANDSHAKING:  return "HANDSHAKING";
        case OZAYN_IPC_CONN_READY:        return "READY";
        case OZAYN_IPC_CONN_FAILED:       return "FAILED";
    }
    return "UNKNOWN";
}

const char *ozayn_ipc_state_name(ozayn_ipc_state_t state) {
    switch (state) {
        case OZAYN_IPC_NOT_CREATED: return "NOT_CREATED";
        case OZAYN_IPC_CREATED:     return "CREATED";
        case OZAYN_IPC_LISTENING:   return "LISTENING";
        case OZAYN_IPC_STOPPING:    return "STOPPING";
        case OZAYN_IPC_STOPPED:     return "STOPPED";
        case OZAYN_IPC_FAILED:      return "FAILED";
    }
    return "UNKNOWN";
}

const char *ozayn_ipc_component_type_name(ozayn_ipc_component_type_t ct) {
    switch (ct) {
        case OZAYN_IPC_COMP_UNKNOWN:  return "UNKNOWN";
        case OZAYN_IPC_COMP_CORE:     return "CORE";
        case OZAYN_IPC_COMP_MODULE:   return "MODULE";
        case OZAYN_IPC_COMP_PLUGIN:   return "PLUGIN";
        case OZAYN_IPC_COMP_WORKER:   return "WORKER";
        case OZAYN_IPC_COMP_EXTERNAL: return "EXTERNAL";
    }
    return "UNKNOWN";
}

/* ================================================================
 * HEADER SERIALIZE / DESERIALIZE
 * ================================================================ */

ozayn_result_t ozayn_ipc_header_pack(const ozayn_ipc_header_t *hdr, uint8_t *buf, size_t buflen) {
    if (!hdr || !buf || buflen < OZAYN_IPC_HEADER_SIZE) return OZAYN_ERR_NULL;

    /* Magic: 2 bytes big-endian */
    buf[0] = (uint8_t)((hdr->magic >> 8) & 0xFF);
    buf[1] = (uint8_t)(hdr->magic & 0xFF);

    /* Version: 1 byte */
    buf[2] = hdr->version;

    /* Type: 1 byte */
    buf[3] = hdr->type;

    /* Flags: 1 byte */
    buf[4] = hdr->flags;

    /* Reserved: 3 bytes */
    buf[5] = 0;
    buf[6] = 0;
    buf[7] = 0;

    /* ID: 4 bytes big-endian */
    buf[8]  = (uint8_t)((hdr->id >> 24) & 0xFF);
    buf[9]  = (uint8_t)((hdr->id >> 16) & 0xFF);
    buf[10] = (uint8_t)((hdr->id >> 8)  & 0xFF);
    buf[11] = (uint8_t)(hdr->id & 0xFF);

    /* Length: 4 bytes big-endian */
    buf[12] = (uint8_t)((hdr->length >> 24) & 0xFF);
    buf[13] = (uint8_t)((hdr->length >> 16) & 0xFF);
    buf[14] = (uint8_t)((hdr->length >> 8)  & 0xFF);
    buf[15] = (uint8_t)(hdr->length & 0xFF);

    return OZAYN_OK;
}

ozayn_result_t ozayn_ipc_header_unpack(ozayn_ipc_header_t *hdr, const uint8_t *buf, size_t buflen) {
    if (!hdr || !buf || buflen < OZAYN_IPC_HEADER_SIZE) return OZAYN_ERR_NULL;

    /* Magic */
    hdr->magic = (uint16_t)((buf[0] << 8) | buf[1]);

    /* Version */
    hdr->version = buf[2];

    /* Type */
    hdr->type = buf[3];

    /* Flags */
    hdr->flags = buf[4];

    /* Reserved */
    hdr->reserved[0] = 0;
    hdr->reserved[1] = 0;
    hdr->reserved[2] = 0;

    /* ID */
    hdr->id = ((uint32_t)buf[8]  << 24) |
              ((uint32_t)buf[9]  << 16) |
              ((uint32_t)buf[10] << 8)  |
              ((uint32_t)buf[11]);

    /* Length */
    hdr->length = ((uint32_t)buf[12] << 24) |
                  ((uint32_t)buf[13] << 16) |
                  ((uint32_t)buf[14] << 8)  |
                  ((uint32_t)buf[15]);

    return OZAYN_OK;
}

/* ================================================================
 * HELPER: Find connection by fd
 * ================================================================ */

static ozayn_ipc_connection_t *find_connection(ozayn_ipc_manager_t *mgr, int fd) {
    for (int i = 0; i < mgr->conn_count; i++) {
        if (mgr->connections[i].fd == fd && mgr->connections[i].state != OZAYN_IPC_CONN_DISCONNECTED)
            return &mgr->connections[i];
    }
    return NULL;
}

/* ================================================================
 * HELPER: Remove connection
 * ================================================================ */

static void remove_connection(ozayn_ipc_connection_t *conn) {
    if (!conn) return;

    LOG_INFO("IPC", "Connection removed: fd=%d id='%s'", conn->fd, conn->id);

    if (conn->fd >= 0) {
        close(conn->fd);
    }

    conn->fd = -1;
    conn->state = OZAYN_IPC_CONN_DISCONNECTED;
    conn->id[0] = '\0';
}

/* ================================================================
 * HELPER: Send raw bytes on a socket
 * ================================================================ */

static ozayn_result_t send_raw(int fd, const uint8_t *data, size_t len) {
    if (fd < 0 || !data) return OZAYN_ERR_NULL;

    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* Wait briefly then retry */
                struct pollfd pfd = { .fd = fd, .events = POLLOUT };
                poll(&pfd, 1, 100);
                continue;
            }
            return OZAYN_ERR;
        }
        sent += (size_t)n;
    }
    return OZAYN_OK;
}

/* ================================================================
 * HELPER: Receive exactly N bytes
 * ================================================================ */

static ozayn_result_t recv_exact(int fd, uint8_t *buf, size_t len, int timeout_ms) {
    if (fd < 0 || !buf) return OZAYN_ERR_NULL;

    size_t received = 0;
    while (received < len) {
        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        int ret = poll(&pfd, 1, timeout_ms);
        if (ret < 0) {
            if (errno == EINTR) continue;
            return OZAYN_ERR;
        }
        if (ret == 0) return OZAYN_ERR; /* timeout */

        ssize_t n = recv(fd, buf + received, len - received, 0);
        if (n <= 0) {
            if (n == 0) return OZAYN_ERR; /* connection closed */
            if (errno == EINTR) continue;
            return OZAYN_ERR;
        }
        received += (size_t)n;
    }
    return OZAYN_OK;
}

/* ================================================================
 * HELPER: Send a complete message (header + payload)
 * ================================================================ */

static ozayn_result_t send_complete_message(int fd, const ozayn_ipc_header_t *hdr,
                                             const uint8_t *payload, uint32_t payload_len) {
    uint8_t header_buf[OZAYN_IPC_HEADER_SIZE];
    ozayn_result_t r = ozayn_ipc_header_pack(hdr, header_buf, sizeof(header_buf));
    if (r != OZAYN_OK) return r;

    r = send_raw(fd, header_buf, OZAYN_IPC_HEADER_SIZE);
    if (r != OZAYN_OK) return r;

    if (payload_len > 0 && payload) {
        r = send_raw(fd, payload, payload_len);
        if (r != OZAYN_OK) return r;
    }

    return OZAYN_OK;
}

/* ================================================================
 * HELPER: Receive a complete message (header + payload)
 * ================================================================ */

static ozayn_result_t recv_complete_message(int fd, ozayn_ipc_message_t *msg, int timeout_ms) {
    if (!msg) return OZAYN_ERR_NULL;

    /* Read header */
    uint8_t header_buf[OZAYN_IPC_HEADER_SIZE];
    ozayn_result_t r = recv_exact(fd, header_buf, sizeof(header_buf), timeout_ms);
    if (r != OZAYN_OK) return r;

    r = ozayn_ipc_header_unpack(&msg->header, header_buf, sizeof(header_buf));
    if (r != OZAYN_OK) return r;

    /* Validate magic */
    if (msg->header.magic != OZAYN_IPC_MAGIC) {
        LOG_WARN("IPC", "Invalid magic: 0x%04X (expected 0x%04X)",
                 msg->header.magic, OZAYN_IPC_MAGIC);
        return OZAYN_ERR;
    }

    /* Validate version */
    if (msg->header.version != OZAYN_IPC_VERSION) {
        LOG_WARN("IPC", "Version mismatch: %d (expected %d)",
                 msg->header.version, OZAYN_IPC_VERSION);
        return OZAYN_ERR;
    }

    /* Validate payload size */
    if (msg->header.length > OZAYN_IPC_MAX_MSG_SIZE) {
        LOG_WARN("IPC", "Payload too large: %u bytes", msg->header.length);
        return OZAYN_ERR;
    }

    /* Free any previous payload */
    free(msg->payload);
    msg->payload = NULL;

    /* Read payload */
    if (msg->header.length > 0) {
        msg->payload = malloc(msg->header.length);
        if (!msg->payload) return OZAYN_ERR;

        r = recv_exact(fd, msg->payload, msg->header.length, timeout_ms);
        if (r != OZAYN_OK) {
            free(msg->payload);
            msg->payload = NULL;
            return r;
        }
    }

    return OZAYN_OK;
}

/* ================================================================
 * HELPER: Process a received message on a connection
 * ================================================================ */

static void process_message(ozayn_ipc_manager_t *mgr, ozayn_ipc_connection_t *conn,
                             ozayn_ipc_message_t *msg) {
    if (!mgr || !conn || !msg) return;

    ozayn_ipc_msg_type_t type = (ozayn_ipc_msg_type_t)msg->header.type;

    LOG_DEBUG("IPC", "Received %s from fd=%d id=%u len=%u",
              ozayn_ipc_msg_type_name(type), conn->fd, msg->header.id, msg->header.length);

    switch (type) {
        case OZAYN_IPC_MSG_HELLO: {
            /* Client handshake: expect payload = [component_type(1) + id_len(1) + id(N)] */
            if (conn->state != OZAYN_IPC_CONN_HANDSHAKING) {
                LOG_WARN("IPC", "HELLO received in state %s, ignoring",
                         ozayn_ipc_conn_state_name(conn->state));
                break;
            }

            if (msg->header.length < 2) {
                LOG_WARN("IPC", "HELLO payload too small (%u bytes), rejecting",
                         msg->header.length);
                /* Send ERROR response */
                ozayn_ipc_header_t err_hdr = {
                    .magic = OZAYN_IPC_MAGIC,
                    .version = OZAYN_IPC_VERSION,
                    .type = OZAYN_IPC_MSG_ERROR,
                    .id = msg->header.id,
                    .length = 0,
                };
                send_complete_message(conn->fd, &err_hdr, NULL, 0);
                conn->state = OZAYN_IPC_CONN_FAILED;
                break;
            }

            /* Parse HELLO payload */
            const uint8_t *p = msg->payload;
            conn->component_type = (ozayn_ipc_component_type_t)p[0];
            uint8_t id_len = p[1];

            if (id_len > 0 && msg->header.length >= (uint32_t)(2 + id_len)) {
                size_t copy_len = id_len < OZAYN_IPC_ID_MAX - 1 ? id_len : OZAYN_IPC_ID_MAX - 1;
                memcpy(conn->id, p + 2, copy_len);
                conn->id[copy_len] = '\0';
            } else {
                snprintf(conn->id, sizeof(conn->id), "client_%d", conn->fd);
            }

            /* Check for duplicate IDs */
            for (int i = 0; i < mgr->conn_count; i++) {
                ozayn_ipc_connection_t *other = &mgr->connections[i];
                if (other != conn && other->state == OZAYN_IPC_CONN_READY &&
                    strcmp(other->id, conn->id) == 0) {
                    LOG_WARN("IPC", "Duplicate component ID: '%s', rejecting", conn->id);
                    ozayn_ipc_header_t err_hdr = {
                        .magic = OZAYN_IPC_MAGIC,
                        .version = OZAYN_IPC_VERSION,
                        .type = OZAYN_IPC_MSG_ERROR,
                        .id = msg->header.id,
                        .length = 0,
                    };
                    send_complete_message(conn->fd, &err_hdr, NULL, 0);
                    conn->state = OZAYN_IPC_CONN_FAILED;
                    break;
                }
            }

            if (conn->state == OZAYN_IPC_CONN_FAILED) break;

            /* --- SECURITY: Authenticate the claimed identity --- */
            if (mgr->security) {
                ozayn_security_manager_t *sec = (ozayn_security_manager_t *)mgr->security;
                ozayn_peer_creds_t peer_creds = {
                    .uid = conn->peer_uid,
                    .gid = conn->peer_gid,
                    .pid = conn->peer_pid,
                    .valid = conn->creds_valid,
                };
                ozayn_auth_result_t auth_result = ozayn_security_authenticate(
                    sec, conn->id, &peer_creds);

                if (auth_result != OZAYN_AUTH_OK) {
                    LOG_WARN("IPC", "Authentication FAILED for '%s' (result=%s, fd=%d)",
                             conn->id, ozayn_auth_result_name(auth_result), conn->fd);
                    ozayn_ipc_header_t err_hdr = {
                        .magic = OZAYN_IPC_MAGIC,
                        .version = OZAYN_IPC_VERSION,
                        .type = OZAYN_IPC_MSG_ERROR,
                        .id = msg->header.id,
                        .length = 0,
                    };
                    send_complete_message(conn->fd, &err_hdr, NULL, 0);
                    conn->state = OZAYN_IPC_CONN_FAILED;

                    if (sec->events) {
                        ozayn_events_publish((ozayn_event_engine_t *)sec->events,
                                             OZAYN_EVENT_ACCESS_DENIED,
                                             OZAYN_SRC_SECURITY, (void *)conn->id);
                    }
                    break;
                }
                conn->authenticated = 1;
                strncpy(conn->identity_id, conn->id, OZAYN_IPC_ID_MAX - 1);
            }
            /* --- End Security Check --- */

            /* Send HELLO_ACK */
            conn->state = OZAYN_IPC_CONN_READY;
            conn->connected_at = time(NULL);

            ozayn_ipc_header_t ack_hdr = {
                .magic = OZAYN_IPC_MAGIC,
                .version = OZAYN_IPC_VERSION,
                .type = OZAYN_IPC_MSG_HELLO_ACK,
                .id = msg->header.id,
                .length = 0,
            };
            send_complete_message(conn->fd, &ack_hdr, NULL, 0);

            LOG_INFO("IPC", "Client connected: '%s' (type=%s, fd=%d)",
                     conn->id,
                     ozayn_ipc_component_type_name(conn->component_type),
                     conn->fd);
            break;
        }

        case OZAYN_IPC_MSG_PING: {
            /* Respond with PONG, same ID */
            ozayn_ipc_header_t pong_hdr = {
                .magic = OZAYN_IPC_MAGIC,
                .version = OZAYN_IPC_VERSION,
                .type = OZAYN_IPC_MSG_PONG,
                .id = msg->header.id,
                .length = 0,
            };
            send_complete_message(conn->fd, &pong_hdr, NULL, 0);
            break;
        }

        case OZAYN_IPC_MSG_BYE: {
            LOG_INFO("IPC", "Client '%s' sent BYE", conn->id);
            conn->state = OZAYN_IPC_CONN_DISCONNECTED;
            break;
        }

        case OZAYN_IPC_MSG_REQUEST: {
            if (conn->state != OZAYN_IPC_CONN_READY) {
                LOG_WARN("IPC", "REQUEST from non-ready connection, ignoring");
                break;
            }

            LOG_INFO("IPC", "Request received from '%s' (id=%u, len=%u)",
                     conn->id, msg->header.id, msg->header.length);

            /* For now, send a generic ACK response */
            /* In future stages, this routes to Command Engine */
            ozayn_ipc_header_t resp_hdr = {
                .magic = OZAYN_IPC_MAGIC,
                .version = OZAYN_IPC_VERSION,
                .type = OZAYN_IPC_MSG_RESPONSE,
                .id = msg->header.id,
                .length = 0,
            };
            send_complete_message(conn->fd, &resp_hdr, NULL, 0);
            break;
        }

        case OZAYN_IPC_MSG_EVENT: {
            if (conn->state != OZAYN_IPC_CONN_READY) {
                LOG_WARN("IPC", "EVENT from non-ready connection, ignoring");
                break;
            }

            LOG_INFO("IPC", "External event received from '%s' (id=%u, len=%u)",
                     conn->id, msg->header.id, msg->header.length);

            /* Future: bridge into internal Event Engine */
            break;
        }

        case OZAYN_IPC_MSG_RESPONSE:
        case OZAYN_IPC_MSG_HELLO_ACK:
        case OZAYN_IPC_MSG_PONG:
        case OZAYN_IPC_MSG_ERROR:
            /* These are response types; not expected from client in normal flow */
            LOG_DEBUG("IPC", "Response-type message %s from fd=%d, noting",
                      ozayn_ipc_msg_type_name(type), conn->fd);
            break;

        default:
            LOG_WARN("IPC", "Unknown message type %d from fd=%d", type, conn->fd);
            break;
    }
}

/* ================================================================
 * INIT
 * ================================================================ */

ozayn_result_t ozayn_ipc_manager_init(ozayn_ipc_manager_t *mgr, const ozayn_ipc_config_t *cfg) {
    if (!mgr || !cfg) return OZAYN_ERR_NULL;

    memset(mgr, 0, sizeof(ozayn_ipc_manager_t));
    mgr->server_fd = -1;
    mgr->events = NULL;
    mgr->recovery = NULL;

    if (!cfg->enabled) {
        mgr->state = OZAYN_IPC_STOPPED;
        mgr->enabled = 0;
        LOG_INFO("IPC", "IPC disabled by configuration");
        return OZAYN_OK;
    }

    mgr->enabled = 1;
    mgr->next_msg_id = 1;

    /* Initialize connection slots */
    for (int i = 0; i < OZAYN_IPC_MAX_CONN; i++) {
        mgr->connections[i].fd = -1;
        mgr->connections[i].state = OZAYN_IPC_CONN_DISCONNECTED;
    }

    /* Ensure endpoint directory exists */
    const char *ep = cfg->endpoint;
    if (!ep || ep[0] == '\0') {
        snprintf(mgr->endpoint, sizeof(mgr->endpoint), "runtime/ipc/ozayn.sock");
    } else {
        snprintf(mgr->endpoint, sizeof(mgr->endpoint), "%s", ep);
    }

    /* Create parent directories for socket */
    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "%s", mgr->endpoint);
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        /* Create all parent directories */
        char tmp[512];
        snprintf(tmp, sizeof(tmp), "%s", dir_path);
        for (char *p = tmp + 1; *p; p++) {
            if (*p == '/') {
                *p = '\0';
                mkdir(tmp, 0755);
                *p = '/';
            }
        }
        mkdir(tmp, 0755);
    }

    /* Remove stale socket file */
    unlink(mgr->endpoint);

    /* Create Unix domain socket */
    mgr->server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (mgr->server_fd < 0) {
        LOG_ERROR("IPC", "Failed to create socket: %s", strerror(errno));
        mgr->state = OZAYN_IPC_FAILED;
        return OZAYN_ERR;
    }

    /* Set non-blocking */
    int flags = fcntl(mgr->server_fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(mgr->server_fd, F_SETFL, flags | O_NONBLOCK);
    }

    /* Bind to endpoint */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%.*s",
             (int)sizeof(addr.sun_path) - 1, mgr->endpoint);

    if (bind(mgr->server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("IPC", "Failed to bind to '%s': %s", mgr->endpoint, strerror(errno));
        close(mgr->server_fd);
        mgr->server_fd = -1;
        mgr->state = OZAYN_IPC_FAILED;
        return OZAYN_ERR;
    }

    /* Listen */
    if (listen(mgr->server_fd, OZAYN_IPC_MAX_CONN) < 0) {
        LOG_ERROR("IPC", "Failed to listen on '%s': %s", mgr->endpoint, strerror(errno));
        close(mgr->server_fd);
        mgr->server_fd = -1;
        unlink(mgr->endpoint);
        mgr->state = OZAYN_IPC_FAILED;
        return OZAYN_ERR;
    }

    mgr->state = OZAYN_IPC_LISTENING;
    LOG_INFO("IPC", "IPC server listening on '%s' (fd=%d)", mgr->endpoint, mgr->server_fd);

    return OZAYN_OK;
}

/* ================================================================
 * SHUTDOWN
 * ================================================================ */

void ozayn_ipc_manager_shutdown(ozayn_ipc_manager_t *mgr) {
    if (!mgr) return;
    if (mgr->state == OZAYN_IPC_STOPPED || mgr->state == OZAYN_IPC_NOT_CREATED) return;

    mgr->state = OZAYN_IPC_STOPPING;
    LOG_INFO("IPC", "IPC manager shutting down");

    /* Send BYE to all connected clients */
    for (int i = 0; i < mgr->conn_count; i++) {
        ozayn_ipc_connection_t *conn = &mgr->connections[i];
        if (conn->state == OZAYN_IPC_CONN_READY && conn->fd >= 0) {
            ozayn_ipc_header_t bye_hdr = {
                .magic = OZAYN_IPC_MAGIC,
                .version = OZAYN_IPC_VERSION,
                .type = OZAYN_IPC_MSG_BYE,
                .id = 0,
                .length = 0,
            };
            send_complete_message(conn->fd, &bye_hdr, NULL, 0);
        }
        remove_connection(conn);
    }

    mgr->conn_count = 0;

    /* Close server socket */
    if (mgr->server_fd >= 0) {
        close(mgr->server_fd);
        mgr->server_fd = -1;
    }

    /* Remove socket file */
    unlink(mgr->endpoint);

    mgr->state = OZAYN_IPC_STOPPED;
    LOG_INFO("IPC", "IPC manager shut down");
}

/* ================================================================
 * BINDINGS
 * ================================================================ */

void ozayn_ipc_manager_set_events(ozayn_ipc_manager_t *mgr, void *events) {
    if (mgr) mgr->events = events;
}

void ozayn_ipc_manager_set_recovery(ozayn_ipc_manager_t *mgr, void *recovery) {
    if (mgr) mgr->recovery = recovery;
}

void ozayn_ipc_manager_set_security(ozayn_ipc_manager_t *mgr, void *security) {
    if (mgr) mgr->security = security;
}

/* ================================================================
 * PROCESS — called from Runtime loop
 * ================================================================ */

ozayn_result_t ozayn_ipc_manager_process(ozayn_ipc_manager_t *mgr) {
    if (!mgr || mgr->state != OZAYN_IPC_LISTENING) return OZAYN_OK;

    /* --- Accept new connections --- */
    if (mgr->conn_count < OZAYN_IPC_MAX_CONN) {
        struct pollfd listen_pfd = { .fd = mgr->server_fd, .events = POLLIN };
        if (poll(&listen_pfd, 1, 0) > 0) {
            int client_fd = accept(mgr->server_fd, NULL, NULL);
            if (client_fd >= 0) {
                /* Set non-blocking */
                int flags = fcntl(client_fd, F_GETFL, 0);
                if (flags >= 0) {
                    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
                }

                /* Find free slot */
                for (int i = 0; i < OZAYN_IPC_MAX_CONN; i++) {
                    if (mgr->connections[i].state == OZAYN_IPC_CONN_DISCONNECTED ||
                        mgr->connections[i].state == OZAYN_IPC_CONN_FAILED) {
                        memset(&mgr->connections[i], 0, sizeof(ozayn_ipc_connection_t));
                        mgr->connections[i].fd = client_fd;
                        mgr->connections[i].state = OZAYN_IPC_CONN_HANDSHAKING;
                        mgr->connections[i].next_msg_id = 1;

                        /* Extract peer credentials (kernel-verified) */
#ifdef __linux__
                        {
                            struct ucred cred;
                            socklen_t clen = sizeof(cred);
                            if (getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED,
                                           &cred, &clen) == 0) {
                                mgr->connections[i].peer_uid = (uint32_t)cred.uid;
                                mgr->connections[i].peer_gid = (uint32_t)cred.gid;
                                mgr->connections[i].peer_pid = (uint32_t)cred.pid;
                                mgr->connections[i].creds_valid = 1;
                                LOG_DEBUG("IPC", "Peer credentials: uid=%u gid=%u pid=%u",
                                          cred.uid, cred.gid, cred.pid);
                            }
                        }
#elif defined(__APPLE__) || defined(__BSD__)
                        {
                            uid_t peer_uid;
                            gid_t peer_gid;
                            if (getpeereid(client_fd, &peer_uid, &peer_gid) == 0) {
                                mgr->connections[i].peer_uid = (uint32_t)peer_uid;
                                mgr->connections[i].peer_gid = (uint32_t)peer_gid;
                                mgr->connections[i].peer_pid = 0; /* not available on macOS */
                                mgr->connections[i].creds_valid = 1;
                            }
                        }
#endif

                        mgr->conn_count++;
                        LOG_INFO("IPC", "New connection accepted (fd=%d, slot=%d, uid=%u)",
                                 client_fd, i, mgr->connections[i].peer_uid);
                        break;
                    }
                }
            }
        }
    }

    /* --- Process existing connections --- */
    for (int i = 0; i < OZAYN_IPC_MAX_CONN; i++) {
        ozayn_ipc_connection_t *conn = &mgr->connections[i];
        if (conn->state == OZAYN_IPC_CONN_DISCONNECTED ||
            conn->state == OZAYN_IPC_CONN_FAILED) {
            continue;
        }

        struct pollfd pfd = { .fd = conn->fd, .events = POLLIN };
        int ret = poll(&pfd, 1, 0);

        if (ret < 0) {
            if (errno == EINTR) continue;
            LOG_WARN("IPC", "Poll error on fd=%d: %s", conn->fd, strerror(errno));
            conn->state = OZAYN_IPC_CONN_FAILED;
            remove_connection(conn);
            mgr->conn_count--;
            continue;
        }

        if (ret == 0) continue; /* no data ready */

        /* Check for hangup */
        if (pfd.revents & (POLLHUP | POLLERR)) {
            LOG_INFO("IPC", "Client '%s' disconnected (fd=%d)", conn->id, conn->fd);
            remove_connection(conn);
            mgr->conn_count--;
            continue;
        }

        /* Data available — receive a complete message */
        if (pfd.revents & POLLIN) {
            /* Peek to see if we have enough data for a header */
            uint8_t peek_buf[OZAYN_IPC_HEADER_SIZE];
            ssize_t peeked = recv(conn->fd, peek_buf, sizeof(peek_buf), MSG_PEEK | MSG_DONTWAIT);
            if (peeked <= 0) {
                if (peeked == 0) {
                    LOG_INFO("IPC", "Client '%s' closed connection (fd=%d)", conn->id, conn->fd);
                    remove_connection(conn);
                    mgr->conn_count--;
                }
                continue;
            }

            /* We have at least some data. Try to read a full message. */
            ozayn_ipc_message_t msg;
            memset(&msg, 0, sizeof(msg));

            /* Use non-blocking recv for header peek, then blocking for rest */
            ozayn_result_t r = recv_complete_message(conn->fd, &msg, 50);
            if (r == OZAYN_OK) {
                process_message(mgr, conn, &msg);
                free(msg.payload);
            } else {
                /* Could not read a complete message within timeout — that's OK, will retry next cycle */
                free(msg.payload);
            }
        }

        /* Remove failed connections */
        if (conn->state == OZAYN_IPC_CONN_FAILED) {
            remove_connection(conn);
            mgr->conn_count--;
        }
    }

    return OZAYN_OK;
}

/* ================================================================
 * SEND MESSAGE (public API)
 * ================================================================ */

ozayn_result_t ozayn_ipc_send_message(ozayn_ipc_manager_t *mgr, int conn_fd,
                                       ozayn_ipc_msg_type_t type,
                                       uint32_t reply_to,
                                       const uint8_t *payload, uint32_t payload_len) {
    if (!mgr || !mgr->enabled) return OZAYN_ERR_STATE;
    if (conn_fd < 0) return OZAYN_ERR_NULL;

    ozayn_ipc_connection_t *conn = find_connection(mgr, conn_fd);
    if (!conn || conn->state != OZAYN_IPC_CONN_READY) return OZAYN_ERR_STATE;

    ozayn_ipc_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = OZAYN_IPC_MAGIC;
    hdr.version = OZAYN_IPC_VERSION;
    hdr.type = (uint8_t)type;
    hdr.id = conn->next_msg_id++;
    hdr.length = payload_len;

    if (reply_to != 0) {
        hdr.id = reply_to; /* response carries original request ID */
    }

    return send_complete_message(conn_fd, &hdr, payload, payload_len);
}

/* ================================================================
 * REQUEST (blocking, with timeout)
 * ================================================================ */

ozayn_result_t ozayn_ipc_request(ozayn_ipc_manager_t *mgr, int conn_fd,
                                  const uint8_t *payload, uint32_t payload_len,
                                  ozayn_ipc_response_t *out) {
    if (!mgr || !out) return OZAYN_ERR_NULL;
    if (!mgr->enabled) return OZAYN_ERR_STATE;

    ozayn_ipc_connection_t *conn = find_connection(mgr, conn_fd);
    if (!conn || conn->state != OZAYN_IPC_CONN_READY) return OZAYN_ERR_STATE;

    /* Send REQUEST */
    uint32_t req_id = conn->next_msg_id++;
    ozayn_ipc_header_t req_hdr;
    memset(&req_hdr, 0, sizeof(req_hdr));
    req_hdr.magic = OZAYN_IPC_MAGIC;
    req_hdr.version = OZAYN_IPC_VERSION;
    req_hdr.type = OZAYN_IPC_MSG_REQUEST;
    req_hdr.id = req_id;
    req_hdr.length = payload_len;

    ozayn_result_t r = send_complete_message(conn_fd, &req_hdr, payload, payload_len);
    if (r != OZAYN_OK) return r;

    /* Wait for RESPONSE */
    memset(out, 0, sizeof(ozayn_ipc_response_t));
    out->request_id = req_id;

    /* Poll for response with timeout */
    time_t start = time(NULL);
    while ((time(NULL) - start) < OZAYN_IPC_REQUEST_TIMEOUT_S) {
        struct pollfd pfd = { .fd = conn_fd, .events = POLLIN };
        int ret = poll(&pfd, 1, 100);
        if (ret <= 0) continue;

        ozayn_ipc_message_t resp;
        memset(&resp, 0, sizeof(resp));
        r = recv_complete_message(conn_fd, &resp, 200);
        if (r != OZAYN_OK) {
            free(resp.payload);
            continue;
        }

        if (resp.header.id == req_id &&
            (resp.header.type == OZAYN_IPC_MSG_RESPONSE ||
             resp.header.type == OZAYN_IPC_MSG_ERROR)) {
            out->response_type = (ozayn_ipc_msg_type_t)resp.header.type;
            out->payload = resp.payload;
            out->payload_len = resp.header.length;
            return OZAYN_OK;
        }

        free(resp.payload);
    }

    LOG_WARN("IPC", "Request %u timed out on fd=%d", req_id, conn_fd);
    return OZAYN_ERR;
}

/* ================================================================
 * QUERY
 * ================================================================ */

int ozayn_ipc_manager_connection_count(const ozayn_ipc_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->conn_count;
}

int ozayn_ipc_manager_is_enabled(const ozayn_ipc_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->enabled;
}
