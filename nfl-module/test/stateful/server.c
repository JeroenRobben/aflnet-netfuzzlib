/*
 * Stateful FTP-like server — demo target for AFLNet + netfuzzlib.
 *
 * A crash is reachable ONLY after the ordered 3-message sequence:
 *     USER <x>\r\n   ->   PASS <y>\r\n   ->   STOR <long>\r\n
 * STOR copies its argument into a fixed 8-byte stack buffer WITHOUT a bounds
 * check, but only while authenticated. Sent in any other state (e.g. STOR
 * first, or STOR before PASS) it is rejected and the buffer is never touched.
 * So no single message can crash it — AFLNet must deliver the full sequence,
 * which exercises the netfuzzlib multi-message queue.
 *
 * Build with AFL_USE_ASAN=1 so AddressSanitizer flags the overflow.
 * Usage: server tcp|udp <port>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

enum { S_INIT, S_USER, S_AUTH };

static void reply(int fd, int is_tcp, struct sockaddr *peer, socklen_t plen,
                  const char *s) {
    if (is_tcp) send(fd, s, strlen(s), 0);
    else        sendto(fd, s, strlen(s), 0, peer, plen);
}

/* Process one command (one message). Returns the next state. */
static int handle(int fd, int is_tcp, struct sockaddr *peer, socklen_t plen,
                  int state, char *line) {
    size_t n = strlen(line);
    while (n && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = 0;

    if (!strncmp(line, "USER ", 5)) {
        reply(fd, is_tcp, peer, plen, "331 need password\r\n");
        return S_USER;
    }
    if (!strncmp(line, "PASS ", 5)) {
        if (state == S_USER) {
            reply(fd, is_tcp, peer, plen, "230 logged in\r\n");
            return S_AUTH;
        }
        reply(fd, is_tcp, peer, plen, "503 bad sequence\r\n");
        return state;
    }
    if (!strncmp(line, "STOR ", 5)) {
        if (state == S_AUTH) {
            char buf[8];                    /* BUG: fixed 8-byte buffer   */
            strcpy(buf, line + 5);          /* overflow if arg > 7 chars  */
            reply(fd, is_tcp, peer, plen, "150 stored\r\n");
            if (buf[0]) reply(fd, is_tcp, peer, plen, "226 done\r\n");
            return state;
        }
        reply(fd, is_tcp, peer, plen, "530 not logged in\r\n");
        return state;
    }
    reply(fd, is_tcp, peer, plen, "500 unknown\r\n");
    return state;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tcp|udp <port>\n", argv[0]); return 2; }
    int is_tcp = !strcmp(argv[1], "tcp");
    int port   = atoi(argv[2]);

    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port   = htons(port);
    a.sin_addr.s_addr = inet_addr("127.0.0.1");

    int s = socket(AF_INET, is_tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
    int one = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if (bind(s, (struct sockaddr *)&a, sizeof(a))) { perror("bind"); return 1; }

    char buf[1024];
    int state = S_INIT;

    if (is_tcp) {
        listen(s, 1);
        int c = accept(s, NULL, NULL);
        if (c < 0) { perror("accept"); return 1; }
        ssize_t r;
        while ((r = recv(c, buf, sizeof(buf) - 1, 0)) > 0) {
            buf[r] = 0;
            state = handle(c, 1, NULL, 0, state, buf);
        }
    } else {
        struct sockaddr_in peer; socklen_t pl = sizeof(peer);
        ssize_t r;
        while ((r = recvfrom(s, buf, sizeof(buf) - 1, 0,
                             (struct sockaddr *)&peer, &pl)) > 0) {
            buf[r] = 0;
            state = handle(s, 0, (struct sockaddr *)&peer, pl, state, buf);
        }
    }
    return 0;
}
