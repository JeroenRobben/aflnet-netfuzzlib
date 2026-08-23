#include "../queue_logic.h"
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    /* --- message pop (count is read live from *count_ptr) --- */
    char payloads[] = { 'A','B','C', 'D','D', 'E' };  /* msg0="ABC" msg1="DD" msg2="E" */
    uint32_t sizes[] = { 3, 2, 1 };
    uint32_t count = 2;                                /* run 1: only 2 messages */
    nfl_msg_cursor cur;
    nfl_msg_cursor_init(&cur, payloads, sizes, &count);

    const char *p; uint32_t n;
    assert(nfl_msg_next(&cur, &p, &n) == 1 && n == 3 && memcmp(p, "ABC", 3) == 0);
    assert(nfl_msg_next(&cur, &p, &n) == 1 && n == 2 && memcmp(p, "DD", 2) == 0);
    assert(nfl_msg_next(&cur, &p, &n) == 0);           /* exhausted at live count 2 */

    /* simulate a new run with a different (larger) live count: re-init cursor,
       bump the shared count. A fresh child would fork with received=0. */
    count = 3;
    nfl_msg_cursor_init(&cur, payloads, sizes, &count);
    assert(nfl_msg_next(&cur, &p, &n) == 1 && n == 3);
    assert(nfl_msg_next(&cur, &p, &n) == 1 && n == 2);
    assert(nfl_msg_next(&cur, &p, &n) == 1 && n == 1 && memcmp(p, "E", 1) == 0);
    assert(nfl_msg_next(&cur, &p, &n) == 0);

    /* --- cumulative response accounting --- */
    uint32_t response_bytes[AFLNET_MAX_MESSAGES] = {0};
    uint32_t total = 0, messages_sent = 0;

    /* early response before any message: attributed to index 0 */
    nfl_record_response(response_bytes, &total, messages_sent, 4);
    assert(total == 4 && response_bytes[0] == 4);

    messages_sent = 1;                              /* delivered msg 0 */
    nfl_record_response(response_bytes, &total, messages_sent, 6);
    assert(total == 10 && response_bytes[0] == 10); /* cumulative, index 0 */

    messages_sent = 2;                              /* delivered msg 1 */
    nfl_record_response(response_bytes, &total, messages_sent, 5);
    assert(total == 15 && response_bytes[1] == 15); /* cumulative, index 1 */

    /* monotonic non-decreasing */
    assert(response_bytes[1] >= response_bytes[0]);

    printf("test_queue_logic OK\n");
    return 0;
}
