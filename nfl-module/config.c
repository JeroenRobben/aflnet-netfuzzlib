#include "config.h"
#include <netfuzzlib/api.h>
#include <netfuzzlib/log.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <string.h>

static aflnet_nfl_state g_state;

aflnet_nfl_state *aflnet_nfl_get_state(void) { return &g_state; }

static void *attach(int id, int flags) {
    void *p = shmat(id, NULL, flags);
    return p; /* caller checks (void*)-1 */
}

int aflnet_nfl_load(void) {
    memset(&g_state, 0, sizeof(g_state));

    const char *id_str = getenv(AFLNET_NFL_SHM_ENV_VAR);
    if (!id_str) { nfl_log("aflnet-nfl: %s not set", AFLNET_NFL_SHM_ENV_VAR); return -1; }

    aflnet_nfl_config *cfg = attach(atoi(id_str), SHM_RDONLY);
    if (cfg == (void *)-1) { nfl_log("aflnet-nfl: attach config failed"); return -1; }

    g_state.protocol = (int)cfg->protocol;

    const struct sockaddr *sut = (const struct sockaddr *)cfg->sockaddr_sut;
    size_t alen = (sut->sa_family == AF_INET) ? sizeof(struct sockaddr_in)
                                              : sizeof(struct sockaddr_in6);
    memcpy(&g_state.sut_addr, cfg->sockaddr_sut, alen);
    memcpy(&g_state.fuzzer_addr, cfg->sockaddr_fuzzer, alen);
    g_state.sut_port_net = (sut->sa_family == AF_INET)
        ? ((struct sockaddr_in *)&g_state.sut_addr)->sin_port
        : ((struct sockaddr_in6 *)&g_state.sut_addr)->sin6_port;

    char     *messages     = attach(cfg->ids.messages, SHM_RDONLY);
    uint32_t *message_sizes = attach(cfg->ids.message_sizes, SHM_RDONLY);
    uint32_t *messages_len  = attach(cfg->ids.messages_len, SHM_RDONLY);
    g_state.response_buf    = attach(cfg->ids.response_buf, 0);
    g_state.response_bytes  = attach(cfg->ids.response_bytes, 0);
    g_state.response_total  = attach(cfg->ids.response_buf_payload_len, 0);
    g_state.messages_sent   = attach(cfg->ids.messages_sent, 0);

    if (messages == (void *)-1 || message_sizes == (void *)-1 || messages_len == (void *)-1 ||
        g_state.response_buf == (void *)-1 || g_state.response_bytes == (void *)-1 ||
        g_state.response_total == (void *)-1 || g_state.messages_sent == (void *)-1) {
        nfl_log("aflnet-nfl: attach segment failed");
        return -1;
    }

    g_state.messages_len = messages_len;
    nfl_msg_cursor_init(&g_state.queue, messages, message_sizes, g_state.messages_len);
    g_state.response_next = g_state.response_buf;
    return 0;
}
