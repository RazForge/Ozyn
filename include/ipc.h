#ifndef OZAYN_IPC_H
#define OZAYN_IPC_H

#include "ozayn.h"
#include <stdint.h>
#include <time.h>

/*
 * ozayn_ipc.h — Inter-Process Communication / Communication Bus.
 *
 * Provides cross-process message passing over Unix domain sockets.
 * Binary protocol with fixed header + variable payload.
 * Supports request/response correlation, event bridging, and handshake.
 */

/* ---- Protocol constants ---- */

#define OZAYN_IPC_MAGIC         0x4F5A  /* "OZ" */
#define OZAYN_IPC_VERSION       1
#define OZAYN_IPC_HEADER_SIZE   16
#define OZAYN_IPC_MAX_MSG_SIZE  (1024 * 1024)  /* 1 MB */
#define OZAYN_IPC_MAX_CONN      16
#define OZAYN_IPC_MAX_PENDING   64
#define OZAYN_IPC_HANDSHAKE_TIMEOUT_S  5
#define OZAYN_IPC_REQUEST_TIMEOUT_S    10
#define OZAYN_IPC_SHUTDOWN_TIMEOUT_S   2
#define OZAYN_IPC_ENDPOINT_MAX  256
#define OZAYN_IPC_ID_MAX        64

/* ---- Message types ---- */

typedef enum {
    OZAYN_IPC_MSG_NONE      = 0,
    OZAYN_IPC_MSG_HELLO     = 1,
    OZAYN_IPC_MSG_HELLO_ACK = 2,
    OZAYN_IPC_MSG_REQUEST   = 3,
    OZAYN_IPC_MSG_RESPONSE  = 4,
    OZAYN_IPC_MSG_EVENT     = 5,
    OZAYN_IPC_MSG_ERROR     = 6,
    OZAYN_IPC_MSG_PING      = 7,
    OZAYN_IPC_MSG_PONG      = 8,
    OZAYN_IPC_MSG_BYE       = 9,
} ozayn_ipc_msg_type_t;

/* ---- Component identity ---- */

typedef enum {
    OZAYN_IPC_COMP_UNKNOWN  = 0,
    OZAYN_IPC_COMP_CORE     = 1,
    OZAYN_IPC_COMP_MODULE   = 2,
    OZAYN_IPC_COMP_PLUGIN   = 3,
    OZAYN_IPC_COMP_WORKER   = 4,
    OZAYN_IPC_COMP_EXTERNAL = 5,
} ozayn_ipc_component_type_t;

/* ---- Message header (16 bytes, fixed) ---- */

typedef struct {
    uint16_t magic;        /* OZAYN_IPC_MAGIC */
    uint8_t  version;      /* protocol version */
    uint8_t  type;         /* ozayn_ipc_msg_type_t */
    uint8_t  flags;        /* reserved */
    uint8_t  reserved[3];  /* padding */
    uint32_t id;           /* unique message ID */
    uint32_t length;       /* payload length in bytes */
} ozayn_ipc_header_t;

/* ---- Message ---- */

typedef struct {
    ozayn_ipc_header_t header;
    uint8_t           *payload;  /* heap-allocated, owned by message */
} ozayn_ipc_message_t;

/* ---- Connection states ---- */

typedef enum {
    OZAYN_IPC_CONN_DISCONNECTED = 0,
    OZAYN_IPC_CONN_CONNECTING   = 1,
    OZAYN_IPC_CONN_HANDSHAKING  = 2,
    OZAYN_IPC_CONN_READY        = 3,
    OZAYN_IPC_CONN_FAILED       = 4,
} ozayn_ipc_conn_state_t;

/* ---- Connection ---- */

typedef struct {
    int                       fd;
    ozayn_ipc_conn_state_t    state;
    char                      id[OZAYN_IPC_ID_MAX];   /* component ID after handshake */
    ozayn_ipc_component_type_t component_type;
    time_t                    connected_at;
    uint32_t                  next_msg_id;             /* for generating message IDs */
    /* Peer credentials (kernel-verified from Unix socket) */
    uint32_t                  peer_uid;
    uint32_t                  peer_gid;
    uint32_t                  peer_pid;
    int                       creds_valid;             /* 1 = kernel-verified */
    /* Security state */
    int                       authenticated;           /* 1 = passed authentication */
    char                      identity_id[OZAYN_IPC_ID_MAX]; /* authenticated identity */
} ozayn_ipc_connection_t;

/* ---- IPC manager state ---- */

typedef enum {
    OZAYN_IPC_NOT_CREATED = 0,
    OZAYN_IPC_CREATED     = 1,
    OZAYN_IPC_LISTENING   = 2,
    OZAYN_IPC_STOPPING    = 3,
    OZAYN_IPC_STOPPED     = 4,
    OZAYN_IPC_FAILED      = 5,
} ozayn_ipc_state_t;

/* ---- Request/Response correlation context ---- */

typedef struct {
    uint32_t  request_id;
    ozayn_ipc_msg_type_t response_type;
    uint8_t  *payload;
    uint32_t  payload_len;
} ozayn_ipc_response_t;

/* ---- IPC manager ---- */

typedef struct {
    ozayn_ipc_state_t      state;
    int                    server_fd;
    char                   endpoint[OZAYN_IPC_ENDPOINT_MAX];
    int                    enabled;
    ozayn_ipc_connection_t connections[OZAYN_IPC_MAX_CONN];
    int                    conn_count;
    uint32_t               next_msg_id;
    void                  *events;   /* event engine pointer (void* to avoid circular include) */
    void                  *recovery; /* recovery context pointer */
    void                  *security; /* security manager pointer */
} ozayn_ipc_manager_t;

/* ---- IPC configuration (parsed from config) ---- */

typedef struct {
    int  enabled;                          /* 0=disabled 1=enabled */
    char endpoint[OZAYN_IPC_ENDPOINT_MAX]; /* socket path */
    int  max_msg_size;                     /* max message size in bytes */
    int  max_connections;                  /* max concurrent connections */
} ozayn_ipc_config_t;

/* ---- Lifecycle ---- */

ozayn_result_t ozayn_ipc_manager_init(ozayn_ipc_manager_t *mgr, const ozayn_ipc_config_t *cfg);
void           ozayn_ipc_manager_shutdown(ozayn_ipc_manager_t *mgr);

/* ---- Bindings ---- */

void ozayn_ipc_manager_set_events(ozayn_ipc_manager_t *mgr, void *events);
void ozayn_ipc_manager_set_recovery(ozayn_ipc_manager_t *mgr, void *recovery);
void ozayn_ipc_manager_set_security(ozayn_ipc_manager_t *mgr, void *security);

/* ---- Runtime integration ---- */

/* Called from Runtime loop: accept connections, receive/dispatch messages */
ozayn_result_t ozayn_ipc_manager_process(ozayn_ipc_manager_t *mgr);

/* ---- Connection management ---- */

int  ozayn_ipc_manager_connection_count(const ozayn_ipc_manager_t *mgr);
int  ozayn_ipc_manager_is_enabled(const ozayn_ipc_manager_t *mgr);

/* ---- Message sending ---- */

/* Send a message to a specific connection */
ozayn_result_t ozayn_ipc_send_message(ozayn_ipc_manager_t *mgr, int conn_fd,
                                       ozayn_ipc_msg_type_t type,
                                       uint32_t reply_to,
                                       const uint8_t *payload, uint32_t payload_len);

/* Send REQUEST, wait for RESPONSE (blocking, with timeout) */
ozayn_result_t ozayn_ipc_request(ozayn_ipc_manager_t *mgr, int conn_fd,
                                  const uint8_t *payload, uint32_t payload_len,
                                  ozayn_ipc_response_t *out);

/* ---- Query ---- */

const char *ozayn_ipc_msg_type_name(ozayn_ipc_msg_type_t type);
const char *ozayn_ipc_conn_state_name(ozayn_ipc_conn_state_t state);
const char *ozayn_ipc_state_name(ozayn_ipc_state_t state);
const char *ozayn_ipc_component_type_name(ozayn_ipc_component_type_t ct);

/* ---- Internal helpers (used by ipc.c) ---- */

ozayn_result_t ozayn_ipc_header_pack(const ozayn_ipc_header_t *hdr, uint8_t *buf, size_t buflen);
ozayn_result_t ozayn_ipc_header_unpack(ozayn_ipc_header_t *hdr, const uint8_t *buf, size_t buflen);

#endif
