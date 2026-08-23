#ifndef NFL_MODULE_AFLNET_CONFIG_H
#define NFL_MODULE_AFLNET_CONFIG_H

#include <netfuzzlib/types.h>
#include "../aflnet_nfl.h"
#include "queue_logic.h"

typedef struct aflnet_nfl_state {
    int protocol;                 /* IPPROTO_TCP | IPPROTO_UDP */
    nfl_addr_t sut_addr;          /* server endpoint */
    nfl_addr_t fuzzer_addr;       /* client endpoint */
    uint16_t sut_port_net;        /* SUT port, network order (fast match) */

    nfl_msg_cursor queue;         /* read cursor over the message shm */
    uint32_t *messages_len;       /* shm: live message count */

    char     *response_buf;       /* shm: concatenated SUT output */
    char     *response_next;      /* write cursor into response_buf */
    uint32_t *response_bytes;     /* shm: cumulative totals */
    uint32_t *response_total;     /* shm: response_buf_payload_len */
    uint32_t *messages_sent;      /* shm: count delivered */
} aflnet_nfl_state;

aflnet_nfl_state *aflnet_nfl_get_state(void);

/* Attach config + all shm segments from AFLNET_NFL_SHM_ENV_VAR. Returns 0 ok. */
int aflnet_nfl_load(void);

#endif /* NFL_MODULE_AFLNET_CONFIG_H */
