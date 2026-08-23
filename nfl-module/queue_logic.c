#include "queue_logic.h"

void nfl_msg_cursor_init(nfl_msg_cursor *c, const char *base,
                         const uint32_t *sizes, const uint32_t *count_ptr) {
    c->base = base;
    c->next = base;
    c->sizes = sizes;
    c->count_ptr = count_ptr;
    c->received = 0;
}

int nfl_msg_next(nfl_msg_cursor *c, const char **out, uint32_t *out_len) {
    if (c->received >= *c->count_ptr)
        return 0;
    uint32_t n = c->sizes[c->received];
    *out = c->next;
    *out_len = n;
    c->next += n;
    c->received++;
    return 1;
}

void nfl_record_response(uint32_t *response_bytes, uint32_t *total,
                         uint32_t messages_sent, uint32_t n) {
    *total += n;
    uint32_t idx = messages_sent ? messages_sent - 1 : 0;
    if (idx < AFLNET_MAX_MESSAGES)
        response_bytes[idx] = *total;   /* cumulative */
}
