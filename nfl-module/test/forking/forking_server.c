/*
 * Build with AFL_USE_ASAN=1 so AddressSanitizer flags the overflow.
 * Usage: forking_server <port>
 */
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

enum { S_INIT, S_USER, S_AUTH };

static void reply(int fd, const char *s) { send(fd, s, strlen(s), 0); }

/* Process one command (one message); returns the next state. */
static int handle(int fd, int state, char *line) {
  size_t n = strlen(line);
  while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
    line[--n] = 0;

  if (!strncmp(line, "USER ", 5)) {
    reply(fd, "331 need password\r\n");
    return S_USER;
  }
  if (!strncmp(line, "PASS ", 5)) {
    if (state == S_USER) {
      reply(fd, "230 logged in\r\n");
      return S_AUTH;
    }
    reply(fd, "503 bad sequence\r\n");
    return state;
  }
  if (!strncmp(line, "STOR ", 5)) {
    if (state == S_AUTH) {
      char buf[8];           /* BUG: fixed 8-byte buffer  */
      strcpy(buf, line + 5); /* overflow if arg > 7 chars */
      reply(fd, "150 stored\r\n");
      if (buf[0])
        reply(fd, "226 done\r\n");
      return state;
    }
    reply(fd, "530 not logged in\r\n");
    return state;
  }
  reply(fd, "500 unknown\r\n");
  return state;
}

int main(int argc, char **argv) {
  int port = argc > 1 ? atoi(argv[1]) : 5599;

  int s = socket(AF_INET, SOCK_STREAM, 0);
  int one = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  struct sockaddr_in a;
  memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  a.sin_addr.s_addr = inet_addr("127.0.0.1");
  if (bind(s, (struct sockaddr *)&a, sizeof(a))) {
    perror("bind");
    return 1;
  }
  listen(s, 4);

  for (;;) {
    int c = accept(s, NULL, NULL);
    if (c < 0)
      break; /* module denies the 2nd accept */
    pid_t pid = fork();
    if (pid == 0) { /* per-connection handler child */
      int state = S_INIT;
      char buf[1024];
      ssize_t r;
      while ((r = recv(c, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[r] = 0;
        state = handle(c, state, buf);
      }
      _exit(0);
    }
    close(c); /* parent keeps accepting */
  }
  return 0;
}
