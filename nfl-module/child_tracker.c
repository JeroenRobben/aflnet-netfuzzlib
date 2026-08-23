#include "child_tracker.h"

#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <netfuzzlib/log.h>

#include "afl_init.h"

enum { AFL_TRACKER_MAX_PIDS = 1024 };

static pid_t tracked_pids[AFL_TRACKER_MAX_PIDS];
static size_t tracked_count;
static int tracking_enabled;
static pthread_mutex_t tracker_mtx = PTHREAD_MUTEX_INITIALIZER;

static pid_t fork_native(void) {
    static pid_t (*real_fork)(void);
    if (!real_fork) {
        real_fork = dlsym(RTLD_NEXT, "fork");
    }
    return real_fork ? real_fork() : -1;
}

static pid_t waitpid_native(pid_t pid, int *status, int options) {
    static pid_t (*real_waitpid)(pid_t, int *, int);
    if (!real_waitpid) {
        real_waitpid = dlsym(RTLD_NEXT, "waitpid");
    }
    return real_waitpid ? real_waitpid(pid, status, options) : -1;
}

void afl_child_tracker_reset(void) {
    pthread_mutex_lock(&tracker_mtx);
    tracked_count = 0;
    tracking_enabled = 1;
    pthread_mutex_unlock(&tracker_mtx);
}

void afl_child_tracker_wait_all(void) {
    pid_t snapshot[AFL_TRACKER_MAX_PIDS];
    size_t n;

    pthread_mutex_lock(&tracker_mtx);
    n = tracked_count;
    for (size_t i = 0; i < n; i++) {
        snapshot[i] = tracked_pids[i];
    }
    tracked_count = 0;
    pthread_mutex_unlock(&tracker_mtx);

    /* If a reaped child died from a fatal signal (a crash in a forked
     * connection handler), remember it. AFLNet only waits on the test-case
     * child, so unless we re-raise the signal here that crash would be lost. */
    int crash_sig = 0;

    for (size_t i = 0; i < n; i++) {
        int status = 0;
        pid_t r = waitpid_native(snapshot[i], &status, 0);
        if (r < 0) {
            /* ECHILD: already reaped (e.g. SUT installed a SIGCHLD handler). */
            nfl_log("afl tracker: waitpid(%d) returned <0", (int)snapshot[i]);
            continue;
        }
        if (WIFSIGNALED(status)) {
            int s = WTERMSIG(status);
            /* Ignore graceful termination; propagate real crashes. */
            if (s != SIGTERM && s != SIGINT)
                crash_sig = s;
        }
        nfl_log("afl tracker: reaped pid %d (status 0x%x)", (int)r, (unsigned)status);
    }

    /* Propagate a handler crash by dying with the same signal, so the fuzzer
     * that waits on this (the test-case) process records the crash. */
    if (crash_sig) {
        nfl_log("afl tracker: propagating child crash signal %d", crash_sig);
        signal(crash_sig, SIG_DFL);
        raise(crash_sig);
    }
}

pid_t fork(void) {
    afl_lazy_manual_init();

    /* Under the deferred forkserver, the snapshot is taken here (in the
     * forkserver parent) just before its first fork. Each forked test-case
     * child returns from afl_lazy_manual_init() with afl_forkserver_child_started
     * set, and then forks its per-connection handler(s). Start a fresh tracking
     * session in the child — once — so it (not the forkserver parent) tracks and
     * reaps those handlers. The forkserver parent never sets the flag, so it
     * doesn't track the test-case children it forks. */
    static int session_active = 0;
    if (afl_forkserver_child_started && !session_active) {
        session_active = 1;
        afl_child_tracker_reset();
    }

    pid_t pid = fork_native();
    if (pid <= 0) {
        return pid;
    }
    pthread_mutex_lock(&tracker_mtx);
    if (tracking_enabled) {
        if (tracked_count < AFL_TRACKER_MAX_PIDS) {
            tracked_pids[tracked_count++] = pid;
        } else {
            nfl_log("afl tracker: capacity %d reached, untracked pid %d",
                         AFL_TRACKER_MAX_PIDS, (int)pid);
        }
    }
    pthread_mutex_unlock(&tracker_mtx);
    return pid;
}
