#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUF 1024

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <host> <port>\n", argv[0]);
        return 1;
    }
    const char *host = argv[1];
    int port = atoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in svr; memset(&svr, 0, sizeof(svr));
    svr.sin_family = AF_INET;
    svr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &svr.sin_addr) != 1) { perror("inet_pton"); return 1; }
    if (connect(sock, (struct sockaddr*)&svr, sizeof(svr)) < 0) { perror("connect"); return 1; }

    printf("Connected. Type lines and press Enter.\n");
    char buf[BUF];
    while (fgets(buf, BUF, stdin)) {
        size_t len = strlen(buf);
        if (write(sock, buf, len) < 0) { perror("write"); break; }
        int n = read(sock, buf, BUF - 1);
        if (n <= 0) break;
        buf[n] = '\0';
        printf("echo: %s", buf);
    }
    close(sock);
    return 0;
}
