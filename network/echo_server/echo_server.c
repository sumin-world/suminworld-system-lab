#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define DEF_PORT 12345
#define BUF 1024

int main(int argc, char *argv[]) {
    int port = (argc > 1) ? atoi(argv[1]) : DEF_PORT;

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return 1; }

    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(s, 16) < 0) { perror("listen"); return 1; }

    printf("Echo server listening on %d\n", port);

    for (;;) {
        struct sockaddr_in cli; socklen_t cl = sizeof(cli);
        int c = accept(s, (struct sockaddr*)&cli, &cl);
        if (c < 0) { perror("accept"); continue; }

        char ip[64]; inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
        printf("Client connected: %s:%d\n", ip, ntohs(cli.sin_port));

        char buf[BUF];
        ssize_t n;
        while ((n = read(c, buf, BUF)) > 0) {
            ssize_t off = 0;
            while (off < n) {
                ssize_t m = write(c, buf + off, n - off);
                if (m < 0) { perror("write"); break; }
                off += m;
            }
        }
        if (n < 0) perror("read");
        close(c);
    }
}
