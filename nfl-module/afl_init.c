#include "afl_init.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>

extern void __afl_manual_init(void) __attribute__((weak));

int afl_forkserver_child_started = 0;

/* Gate: only fire under afl-fuzz with deferred forkserver enabled. */
void afl_lazy_manual_init(void) {
  static int done = 0;
  if (done)
    return;
  done = 1;
  if (&__afl_manual_init != NULL && getenv("__AFL_DEFER_FORKSRV")) {
    __afl_manual_init();
    /* Reached only in a test-case child (the forkserver parent blocks in
     * __afl_manual_init and never returns here). */
    afl_forkserver_child_started = 1;
  }
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg) {
  static int (*real)(pthread_t *, const pthread_attr_t *, void *(*)(void *),
                     void *);
  if (!real) {
    real = dlsym(RTLD_NEXT, "pthread_create");
  }
  afl_lazy_manual_init();
  return real(thread, attr, start_routine, arg);
}
