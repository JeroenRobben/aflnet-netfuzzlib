#ifndef AFLNET_NFL_FUZZER_H
#define AFLNET_NFL_FUZZER_H

#include "types.h"
#include "aflnet.h"
#include "klist.h"

/* Allocate all shm segments, fill the config, set AFLNET_NFL_SHM_ENV_VAR.
 * sut_ip is a dotted/colon string; ports are host order. Returns 0 on success. */
int setup_aflnet_nfl(u8 protocol, const u8 *sut_ip, u32 sut_port, u32 fuzzer_port);

/* Attach the response segments so the fuzzer reads them directly.
 * response_buf and response_bytes point into shm after this call. */
void attach_aflnet_nfl_response_shmem(char **response_buf,
                                      int **response_buf_size_ptr_out,
                                      u32 **response_bytes,
                                      u32 **messages_sent_ptr_out);

/* Copy the full kl_messages list into the message-queue shm. */
void sync_messages_to_shmem(klist_t(lms) *kl_messages);

/* Zero response_bytes, response_buf_payload_len, messages_sent (before a run). */
void reset_response_shmem(void);

/* After a run: copy messages_sent and total response size out of shm into
 * the caller-supplied globals. */
void readback_response_shmem(u32 *messages_sent, int *response_buf_size);

#endif /* AFLNET_NFL_FUZZER_H */
