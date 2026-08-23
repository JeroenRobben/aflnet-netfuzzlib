#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <netfuzzlib/api.h>
#include <netfuzzlib/callbacks.h>
#include <netfuzzlib/log.h>

#include "afl_init.h"
#include "child_tracker.h"
#include "config.h"

static bool endpoint_granted; /* one connection per run */

static bool session_started;

static bool addr_port_is(const nfl_addr_t *a, uint16_t port_net) {
  if (!a)
    return false;
  if (a->s.sa_family == AF_INET)
    return a->s4.sin_port == port_net;
  if (a->s.sa_family == AF_INET6)
    return a->s6.sin6_port == port_net;
  return false;
}

/* Begin tracking SUT-spawned children for this test case, once. Skipped under
 * the deferred forkserver, where the snapshot itself is the reset point. */
static void afl_begin_session(void) {
  if (session_started)
    return;
  session_started = true;
  if (!getenv("__AFL_DEFER_FORKSRV"))
    afl_child_tracker_reset();
}

int nfl_setup(void) {
  if (aflnet_nfl_load() != 0)
    return -1;
  return 0;
}

bool nfl_tcp_connect(const nfl_sock_t *sock, const nfl_addr_t *remote_addr) {
  (void)sock;
  aflnet_nfl_state *st = aflnet_nfl_get_state();
  if (st->protocol != IPPROTO_TCP)
    return false;
  if (!addr_port_is(remote_addr, st->sut_port_net))
    return false;
  if (endpoint_granted)
    return false;
  endpoint_granted = true;
  nfl_log("aflnet-nfl: connect granted");
  return true;
}

bool nfl_tcp_accept(const nfl_sock_t *sock, nfl_addr_t *remote_addr) {
  aflnet_nfl_state *st = aflnet_nfl_get_state();
  if (st->protocol != IPPROTO_TCP)
    return false;
  if (!addr_port_is(sock->local_addr, st->sut_port_net))
    return false;
  if (endpoint_granted)
    return false;
  afl_begin_session();
  endpoint_granted = true;
  size_t alen = (sock->domain == AF_INET) ? sizeof(struct sockaddr_in)
                                          : sizeof(struct sockaddr_in6);
  memcpy(remote_addr, &st->fuzzer_addr, alen);
  nfl_log("aflnet-nfl: accept granted");
  return true;
}

nfl_conn_result nfl_send(const nfl_sock_t *sock, const nfl_addr_t *to,
                         const struct iovec *iov, size_t iovlen) {
  (void)sock;
  (void)to;
  aflnet_nfl_state *st = aflnet_nfl_get_state();
  for (size_t i = 0; i < iovlen; i++) {
    size_t n = iov[i].iov_len;
    /* guard shm bound (leave 1 byte for the NUL below) */
    size_t used = (size_t)(st->response_next - st->response_buf);
    if (used + n + 1 > AFLNET_MESSAGE_QUEUE_SIZE)
      n = AFLNET_MESSAGE_QUEUE_SIZE - used - 1;
    if (n == 0)
      break;
    memcpy(st->response_next, iov[i].iov_base, n);
    st->response_next += n;
    nfl_record_response(st->response_bytes, st->response_total,
                        *st->messages_sent, (uint32_t)n);
  }
  *st->response_next = '\0';
  return NFL_CONN_OK;
}

nfl_conn_result nfl_receive(const nfl_sock_t *sock, nfl_pkt **pkt,
                            nfl_recv_info *info) {
  *pkt = NULL;
  aflnet_nfl_state *st = aflnet_nfl_get_state();

  /* only the fuzz socket (SUT port + matching protocol) */
  if (sock->protocol != st->protocol)
    return NFL_CONN_OK;
  if (!addr_port_is(sock->local_addr, st->sut_port_net) &&
      !addr_port_is(sock->remote_addr, st->sut_port_net))
    return NFL_CONN_OK;

  /* UDP has no accept(); start the child-tracking session on first receive. */
  afl_begin_session();

  /* SnapFuzz latest-safe-point forkserver */
  afl_lazy_manual_init();

  const char *data;
  uint32_t len;
  if (!nfl_msg_next(&st->queue, &data, &len)) {
    /* exhausted */
    return (st->protocol == IPPROTO_TCP) ? NFL_CONN_CLOSED : NFL_CONN_OK;
  }

  nfl_pkt *p = nfl_alloc_pkt(len);
  if (!p)
    return NFL_CONN_OK;
  if (len)
    memcpy(p->buf, data, len);
  *pkt = p;
  (*st->messages_sent)++;

  if (st->protocol != IPPROTO_TCP) {
    memcpy(&info->src_addr, &st->fuzzer_addr, sizeof(nfl_addr_t));
    memcpy(&info->dst_addr, &st->sut_addr, sizeof(nfl_addr_t));
    info->iface_index = 1;
  }
  return NFL_CONN_OK;
}

void nfl_socket_idle(const nfl_sock_t *sock) {
  (void)sock;
  if (!nfl_all_sockets_in_process_idle())
    return;
  nfl_log("aflnet-nfl: all sockets idle, reaping children and exiting");
  afl_child_tracker_wait_all();
  exit(0);
}
