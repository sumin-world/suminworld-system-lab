#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <signal.h>

#define BUF 1024
#define BACKLOG 16
#define MAXFD  FD_SETSIZE

static void tune_socket(int fd){
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
}
static void ignore_sigpipe(void){
    struct sigaction sa = {0};
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
}

int main(int argc, char *argv[]){
    if (argc != 2){ fprintf(stderr,"Usage: %s <PORT>\n", argv[0]); return 1; }
    int port = atoi(argv[1]);
    ignore_sigpipe();

    int serv = socket(AF_INET, SOCK_STREAM, 0);
    if (serv < 0){ perror("socket"); return 1; }
    tune_socket(serv);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(serv, (struct sockaddr*)&addr, sizeof(addr)) < 0){ perror("bind"); return 1; }
    if (listen(serv, BACKLOG) < 0){ perror("listen"); return 1; }

    fd_set allset, rset;
    int client[MAXFD]; for (int i=0;i<MAXFD;i++) client[i] = -1;
    FD_ZERO(&allset);
    FD_SET(serv, &allset);
    int maxfd = serv;

    char buf[BUF];
    printf("TCP select-echo on :%d\n", port);

    while (1){
        rset = allset;
        int nready = select(maxfd+1, &rset, NULL, NULL, NULL);
        if (nready < 0){ perror("select"); continue; }

        // 새 연결 수락
        if (FD_ISSET(serv, &rset)){
            struct sockaddr_in cli; socklen_t clen = sizeof(cli);
            int cfd = accept(serv, (struct sockaddr*)&cli, &clen);
            if (cfd >= 0){
                // ---- 추가된 로그: 클라이언트 정보 출력 ----
                char ip[INET_ADDRSTRLEN] = {0};
                inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
                printf("Accepted: %s:%d (fd=%d)\n", ip, ntohs(cli.sin_port), cfd);
                // ----------------------------------------

                FD_SET(cfd, &allset);
                if (cfd > maxfd) maxfd = cfd;
                for (int i=0;i<MAXFD;i++){
                    if (client[i] < 0){ client[i] = cfd; break; }
                }
            } else {
                perror("accept");
            }
            if (--nready <= 0) continue;
        }

        // 기존 클라이언트 소켓 처리
        for (int i=0;i<MAXFD;i++){
            int fd = client[i];
            if (fd < 0) continue;
            if (FD_ISSET(fd, &rset)){
                ssize_t n = read(fd, buf, sizeof(buf));
                if (n <= 0){
                    // ---- 연결 종료 로그 ----
                    printf("Closed: fd=%d\n", fd);
                    // -----------------------
                    close(fd);
                    FD_CLR(fd, &allset);
                    client[i] = -1;
                } else {
                    ssize_t off = 0;
                    while (off < n){
                        ssize_t m = write(fd, buf+off, n-off);
                        if (m <= 0){
                            perror("write");
                            close(fd);
                            FD_CLR(fd, &allset);
                            client[i] = -1;
                            break;
                        }
                        off += m;
                    }
                }
                if (--nready <= 0) break;
            }
        }
    }
    return 0;
}
