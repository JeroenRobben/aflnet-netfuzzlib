#ifndef NFL_MODULE_QUEUE_LOGIC_H
#define NFL_MODULE_QUEUE_LOGIC_H

#include <stdint.h>
#include "../aflnet_nfl.h"

/* Read cursor over a concatenated message buffer. Holds no ownership. */
typedef struct nfl_msg_cursor {
    const char *base;        /* start of concatenated payloads */
    const char *next;        /* next unread byte */
    const uint32_t *sizes;   /* per-message sizes */
    const uint32_t *count_ptr;   /* live message count (points into shm) */
    uint32_t received;       /* messages already popped */
} nfl_msg_cursor;

void nfl_msg_cursor_init(nfl_msg_cursor *c, const char *base,
                         const uint32_t *sizes, const uint32_t *count_ptr);

/* Pop the next message. On success sets out and out_len and returns 1.
 * Returns 0 when the queue is exhausted (leaves outputs untouched).
 * The message count is read live from *count_ptr on each call. */
int nfl_msg_next(nfl_msg_cursor *c, const char **out, uint32_t *out_len);

/* Record `n` response bytes the SUT just sent. Maintains a running total
 * and writes the CUMULATIVE total into response_bytes at the slot for the
 * current message (messages_sent-1, or 0 before the first message), matching
 * latest aflnet send_over_network semantics. */
void nfl_record_response(uint32_t *response_bytes, uint32_t *total,
                         uint32_t messages_sent, uint32_t n);

#endif /* NFL_MODULE_QUEUE_LOGIC_H */
