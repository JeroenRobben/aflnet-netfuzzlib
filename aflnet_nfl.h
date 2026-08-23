#ifndef AFLNET_NFL_H
#define AFLNET_NFL_H

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define AFLNET_NFL_SHM_ENV_VAR "__AFLNET_NFL_SHM_ID"

#define AFLNET_MAX_MESSAGES        256
#define AFLNET_MESSAGE_QUEUE_SIZE  (8 * 4096 * AFLNET_MAX_MESSAGES)

/* System-V shm ids for every segment, stored inside the config segment. */
typedef struct aflnet_nfl_shm_ids {
    int messages;                 /* char[AFLNET_MESSAGE_QUEUE_SIZE] */
    int message_sizes;            /* uint32_t[AFLNET_MAX_MESSAGES]   */
    int messages_len;             /* uint32_t (message count)        */
    int response_buf;             /* char[AFLNET_MESSAGE_QUEUE_SIZE] */
    int response_bytes;           /* uint32_t[AFLNET_MAX_MESSAGES] (CUMULATIVE) */
    int response_buf_payload_len; /* uint32_t (total response bytes) */
    int messages_sent;            /* uint32_t                        */
} aflnet_nfl_shm_ids;

/* The config segment itself; its id is passed via AFLNET_NFL_SHM_ENV_VAR. */
typedef struct aflnet_nfl_config {
    uint32_t protocol;   /* IPPROTO_TCP | IPPROTO_UDP */
    char sockaddr_sut[sizeof(struct sockaddr_in6)];    /* SUT (server) endpoint */
    char sockaddr_fuzzer[sizeof(struct sockaddr_in6)]; /* fuzzer (client) endpoint */
    aflnet_nfl_shm_ids ids;
} aflnet_nfl_config;

#endif /* AFLNET_NFL_H */
