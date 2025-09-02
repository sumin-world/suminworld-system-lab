// multi_echo_server.c - select()를 사용한 멀티클라이언트 Echo 서버
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h> // struct timeval (BSD/macOS 호환)

#ifdef __linux__
#include <sys/socket.h>  // accept4
#include <netinet/tcp.h>  // TCP_KEEPIDLE, TCP_KEEPINTVL, TCP_KEEPCNT
#endif

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

typedef struct {
    int fd;
    struct sockaddr_in addr;
    char ip[INET_ADDRSTRLEN];
} client_t;

static int send_all(int fd, const char *buf, size_t len) {
    while (len) {
        ssize_t n = send(fd, buf, len, 0);  // MSG_NOSIGNAL 제거 (signal 처리로 충분)
        if (n < 0) { 
            if (errno == EINTR) continue; 
            return -1; 
        }
        if (n == 0) return -1;
        buf += n; 
        len -= (size_t)n;
    }
    return 0;
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    setvbuf(stdout, NULL, _IONBF, 0);  // 즉시 출력

    // 서버 소켓 생성
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 128) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("[*] 멀티클라이언트 Echo 서버 시작 (포트 %d)\n", PORT);
    printf("[*] 최대 동시 접속: %d (FD_SETSIZE 제한: %d)\n", MAX_CLIENTS, FD_SETSIZE);

    // 클라이언트 배열 초기화
    client_t clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
    }

    fd_set read_fds, master_fds;
    int max_fd = server_fd;

    FD_ZERO(&master_fds);
    FD_SET(server_fd, &master_fds);

    char buffer[BUFFER_SIZE];
    struct timeval timeout;

    while (1) {
        read_fds = master_fds;  // 매번 복사 (select가 수정하므로)
        
        // 60초 타임아웃 설정 (유휴 연결 체크용)
        timeout.tv_sec = 60;
        timeout.tv_usec = 0;

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (activity < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }
        
        if (activity == 0) {
            // 타임아웃 - 유휴 연결 체크 (여기서는 단순 로그만)
            printf("[*] select 타임아웃 (60초) - 연결 상태 체크\n");
            continue;
        }

        // 1. 새로운 연결 확인
        if (FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
#ifdef __linux__
            int new_client = accept4(server_fd, (struct sockaddr*)&client_addr, &client_len, SOCK_CLOEXEC);
#else
            int new_client = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (new_client >= 0) fcntl(new_client, F_SETFD, FD_CLOEXEC);
#endif
            
            if (new_client < 0) {
                if (errno != EINTR) perror("accept");
                continue;
            }

            // FD_SETSIZE 가드 (중요!)
            if (new_client >= FD_SETSIZE) {
                printf("[!] fd %d >= FD_SETSIZE(%d) - 연결 거부\n", new_client, FD_SETSIZE);
                close(new_client);
                continue;
            }

            // 빈 슬롯 찾기
            int slot = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == -1) {
                    slot = i;
                    break;
                }
            }

            if (slot == -1) {
                printf("[!] 최대 클라이언트 수 초과 - 연결 거부\n");
                close(new_client);
            } else {
                // Keepalive 설정 (장기 연결 테스트용)
                int ka = 1;
                if (setsockopt(new_client, SOL_SOCKET, SO_KEEPALIVE, &ka, sizeof(ka)) < 0)
                    perror("SO_KEEPALIVE");
                
#ifdef __linux__
                // Linux 전용 TCP Keep-Alive 세부 설정
                int idle = 60, intvl = 10, cnt = 5;
                if (setsockopt(new_client, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,  sizeof(idle))  < 0) perror("TCP_KEEPIDLE");
                if (setsockopt(new_client, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl)) < 0) perror("TCP_KEEPINTVL");
                if (setsockopt(new_client, IPPROTO_TCP, TCP_KEEPCNT,   &cnt,   sizeof(cnt))   < 0) perror("TCP_KEEPCNT");
#endif
                
                // Send/Recv 타임아웃 설정
                struct timeval snd_to = { .tv_sec = 2,   .tv_usec = 0 };
                struct timeval rcv_to = { .tv_sec = 120, .tv_usec = 0 };
                if (setsockopt(new_client, SOL_SOCKET, SO_SNDTIMEO, &snd_to, sizeof(snd_to)) < 0) perror("SO_SNDTIMEO");
                if (setsockopt(new_client, SOL_SOCKET, SO_RCVTIMEO, &rcv_to, sizeof(rcv_to)) < 0) perror("SO_RCVTIMEO");
                
                // 클라이언트 등록
                clients[slot].fd = new_client;
                clients[slot].addr = client_addr;
                inet_ntop(AF_INET, &client_addr.sin_addr, clients[slot].ip, INET_ADDRSTRLEN);

                FD_SET(new_client, &master_fds);
                if (new_client > max_fd) max_fd = new_client;

                printf("[+] 클라이언트 #%d 연결: %s:%d (fd=%d)\n", 
                       slot, clients[slot].ip, ntohs(client_addr.sin_port), new_client);

                // 현재 접속자 수 출력
                int count = 0;
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].fd != -1) count++;
                }
                printf("[*] 현재 접속자: %d/%d\n", count, MAX_CLIENTS);
            }
        }

        // 2. 기존 클라이언트들의 데이터 확인
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int client_fd = clients[i].fd;
            if (client_fd == -1) continue;

            if (FD_ISSET(client_fd, &read_fds)) {
                ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);
                
                if (bytes_read > 0) {
                    // 데이터 수신 성공
                    printf("[<] 클라이언트 #%d로부터 %zd bytes: ", i, bytes_read);
                    fwrite(buffer, 1, (size_t)bytes_read, stdout);
                    printf("\n");
                    fflush(stdout);

                    // Echo back (주의: send_all은 블로킹 가능성 있음)
                    if (send_all(client_fd, buffer, (size_t)bytes_read) < 0) {
                        printf("[!] 클라이언트 #%d 전송 실패 - 연결 해제\n", i);
                        goto cleanup_client;
                    }
                    printf("[>] 클라이언트 #%d에게 %zd bytes 전송 완료\n", i, bytes_read);

                } else if (bytes_read == 0) {
                    // 정상 연결 해제
                    printf("[!] 클라이언트 #%d 연결 종료 (%s)\n", i, clients[i].ip);
                    goto cleanup_client;

                } else {
                    // 에러
                    if (errno != EINTR) {
                        printf("[!] 클라이언트 #%d 수신 오류 - 연결 해제\n", i);
                        goto cleanup_client;
                    }
                }
                continue;

cleanup_client:
                // 클라이언트 정리
                FD_CLR(client_fd, &master_fds);
                close(client_fd);
                clients[i].fd = -1;
                
                // max_fd 재계산
                if (client_fd == max_fd) {
                    max_fd = server_fd;
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].fd > max_fd) {
                            max_fd = clients[j].fd;
                        }
                    }
                }

                // 현재 접속자 수 출력
                int count = 0;
                for (int j = 0; j < MAX_CLIENTS; j++) {
                    if (clients[j].fd != -1) count++;
                }
                printf("[*] 현재 접속자: %d/%d\n", count, MAX_CLIENTS);
            }
        }
    }

    // 정리
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != -1) {
            close(clients[i].fd);
        }
    }
    close(server_fd);
    return 0;
}