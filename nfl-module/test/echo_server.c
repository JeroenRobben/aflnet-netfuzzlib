/* Minimal TCP server: accept one connection, echo lines until EOF. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main(int argc, char **argv) {
    int port = argc > 1 ? atoi(argv[1]) : 5555;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_port = htons(port);
    a.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(s, (struct sockaddr *)&a, sizeof(a))) { perror("bind"); return 1; }
    listen(s, 1);
    int c = accept(s, NULL, NULL);
    if (c < 0) { perror("accept"); return 1; }
    char buf[1024]; ssize_t n;
    while ((n = read(c, buf, sizeof(buf))) > 0) {
        if (write(c, buf, n) != n) break;   /* echo -> becomes response_buf */
    }
    return 0;
}
