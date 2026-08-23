#include "aflnet-nfl.h"
#include "aflnet_nfl.h"
#include "debug.h"
#include "alloc-inl.h"
#include <sys/shm.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>

static aflnet_nfl_config *cfg = NULL;

static char     *msg_shmem;         /* attached message queue segments */
static u32      *msg_sizes_shmem;
static u32      *msg_len_shmem;
static u32      *response_bytes_shmem;
static u32      *response_buf_payload_len_shmem;
static u32      *messages_sent_shmem;

static int shm_new(size_t sz) {
    int id = shmget(IPC_PRIVATE, sz, IPC_CREAT | IPC_EXCL | 0666);
    if (id < 0) PFATAL("shmget() failed");
    return id;
}

int setup_aflnet_nfl(u8 protocol, const u8 *sut_ip, u32 sut_port, u32 fuzzer_port) {
    int cfg_id = shm_new(sizeof(aflnet_nfl_config));
    cfg = shmat(cfg_id, NULL, 0);
    if (cfg == (void *)-1) PFATAL("shmat config failed");
    memset(cfg, 0, sizeof(*cfg));

    cfg->protocol = (protocol == PRO_TCP) ? IPPROTO_TCP : IPPROTO_UDP;

    cfg->ids.messages                 = shm_new(AFLNET_MESSAGE_QUEUE_SIZE);
    cfg->ids.message_sizes            = shm_new(AFLNET_MAX_MESSAGES * sizeof(u32));
    cfg->ids.messages_len             = shm_new(sizeof(u32));
    cfg->ids.response_buf             = shm_new(AFLNET_MESSAGE_QUEUE_SIZE);
    cfg->ids.response_bytes           = shm_new(AFLNET_MAX_MESSAGES * sizeof(u32));
    cfg->ids.response_buf_payload_len = shm_new(sizeof(u32));
    cfg->ids.messages_sent            = shm_new(sizeof(u32));

    /* endpoints: SUT (server) + fuzzer (client, loopback) */
    struct sockaddr_in  *s4_sut = (struct sockaddr_in *)cfg->sockaddr_sut;
    struct sockaddr_in  *s4_fz  = (struct sockaddr_in *)cfg->sockaddr_fuzzer;
    struct sockaddr_in6 *s6_sut = (struct sockaddr_in6 *)cfg->sockaddr_sut;
    struct sockaddr_in6 *s6_fz  = (struct sockaddr_in6 *)cfg->sockaddr_fuzzer;

    if (inet_pton(AF_INET, (const char *)sut_ip, &s4_sut->sin_addr) == 1) {
        s4_sut->sin_family = AF_INET;
        s4_sut->sin_port   = htons(sut_port);
        s4_fz->sin_family  = AF_INET;
        s4_fz->sin_port    = fuzzer_port ? htons(fuzzer_port) : 0;
        s4_fz->sin_addr.s_addr = inet_addr("127.0.0.1");
    } else if (inet_pton(AF_INET6, (const char *)sut_ip, &s6_sut->sin6_addr) == 1) {
        s6_sut->sin6_family = AF_INET6;
        s6_sut->sin6_port   = htons(sut_port);
        s6_fz->sin6_family  = AF_INET6;
        s6_fz->sin6_port    = fuzzer_port ? htons(fuzzer_port) : 0;
        struct in6_addr lo = IN6ADDR_LOOPBACK_INIT;
        memcpy(&s6_fz->sin6_addr, &lo, sizeof(lo));
    } else {
        FATAL("Invalid SUT IP for netfuzzlib: %s", sut_ip);
    }

    /* attach message-queue segments the fuzzer writes to */
    msg_shmem       = shmat(cfg->ids.messages, NULL, 0);
    msg_sizes_shmem = shmat(cfg->ids.message_sizes, NULL, 0);
    msg_len_shmem   = shmat(cfg->ids.messages_len, NULL, 0);
    if (msg_shmem == (void *)-1 || msg_sizes_shmem == (void *)-1 || msg_len_shmem == (void *)-1)
        PFATAL("shmat message queue failed");

    char *id_str = alloc_printf("%d", cfg_id);
    setenv(AFLNET_NFL_SHM_ENV_VAR, id_str, 1);
    ck_free(id_str);

    OKF("netfuzzlib shm init done (proto=%s port=%u)",
        protocol == PRO_TCP ? "tcp" : "udp", sut_port);
    return 0;
}

void attach_aflnet_nfl_response_shmem(char **response_buf,
                                      int **response_buf_size_ptr_out,
                                      u32 **response_bytes,
                                      u32 **messages_sent_ptr_out) {
    *response_buf                  = shmat(cfg->ids.response_buf, NULL, 0);
    response_bytes_shmem           = shmat(cfg->ids.response_bytes, NULL, 0);
    response_buf_payload_len_shmem = shmat(cfg->ids.response_buf_payload_len, NULL, 0);
    messages_sent_shmem            = shmat(cfg->ids.messages_sent, NULL, 0);
    if (*response_buf == (void *)-1 || response_bytes_shmem == (void *)-1 ||
        response_buf_payload_len_shmem == (void *)-1 || messages_sent_shmem == (void *)-1)
        PFATAL("shmat response segments failed");

    *response_bytes           = response_bytes_shmem;
    *response_buf_size_ptr_out = NULL;   /* fuzzer keeps its own int; see readback */
    *messages_sent_ptr_out     = messages_sent_shmem;
}

void sync_messages_to_shmem(klist_t(lms) *kl_messages) {
    *msg_len_shmem = 0;
    memset(msg_sizes_shmem, 0, AFLNET_MAX_MESSAGES * sizeof(u32));
    char *w = msg_shmem;
    size_t used = 0;
    kliter_t(lms) *it;
    for (it = kl_begin(kl_messages); it != kl_end(kl_messages); it = kl_next(it)) {
        u32 sz = kl_val(it)->msize;
        if (*msg_len_shmem >= AFLNET_MAX_MESSAGES ||
            used + sz > AFLNET_MESSAGE_QUEUE_SIZE) {
            WARNF("netfuzzlib: message queue full, truncating sequence");
            break;
        }
        memcpy(w, kl_val(it)->mdata, sz);
        w += sz; used += sz;
        msg_sizes_shmem[*msg_len_shmem] = sz;
        (*msg_len_shmem)++;
    }
}

void reset_response_shmem(void) {
    memset(response_bytes_shmem, 0, AFLNET_MAX_MESSAGES * sizeof(u32));
    *response_buf_payload_len_shmem = 0;
    *messages_sent_shmem = 0;
}

void readback_response_shmem(u32 *messages_sent, int *response_buf_size) {
    *messages_sent     = *messages_sent_shmem;
    *response_buf_size = (int)*response_buf_payload_len_shmem;
}
