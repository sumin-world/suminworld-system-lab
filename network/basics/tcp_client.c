// tcp_client.c  (non-blocking connect + timeout + 재시도)
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>

static int connect_with_timeout(int sock, const struct sockaddr *sa, socklen_t slen, int ms) {
    // non-blocking으로 전환
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return -1;
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) return -1;

    int rc = connect(sock, sa, slen);
    if (rc == 0) { // 즉시 성공
        fcntl(sock, F_SETFL, flags);
        return 0;
    }
    if (errno != EINPROGRESS) return -1;

    // select로 타임아웃 대기
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(sock, &wfds);
    struct timeval tv = { .tv_sec = ms/1000, .tv_usec = (ms%1000)*1000 };

    rc = select(sock+1, NULL, &wfds, NULL, &tv);
    if (rc <= 0) { errno = (rc==0 ? ETIMEDOUT : errno); return -1; }

    int soerr = 0; socklen_t len = sizeof(soerr);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &soerr, &len) < 0) return -1;
    if (soerr != 0) { errno = soerr; return -1; }

    // 블로킹 모드로 되돌림
    fcntl(sock, F_SETFL, flags);
    return 0;
}

static int fetch_http(const char *host, const char *port, const char *req_host_hdr) {
    printf("[*] DNS: %s:%s\n", host, port);
    struct addrinfo hints = {0}, *res=NULL, *ai=NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int gai = getaddrinfo(host, port, &hints, &res);
    if (gai != 0) { fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai)); return -1; }

    int sock = -1, ok = -1;
    for (ai = res; ai; ai = ai->ai_next) {
        struct sockaddr_in *in = (struct sockaddr_in*)ai->ai_addr;
        char ip[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &in->sin_addr, ip, sizeof(ip));
        printf("[*] 시도 IP=%s ...\n", ip);

        sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sock < 0) { perror("socket"); continue; }

        // 3초 타임아웃 connect
        if (connect_with_timeout(sock, ai->ai_addr, ai->ai_addrlen, 3000) == 0) {
            printf("[+] 연결 성공 (%s)\n", ip);
            ok = 0;
            break;
        } else {
            perror("connect");
            close(sock); sock = -1;
        }
    }
    freeaddrinfo(res);
    if (ok != 0) return -1;

    const char *req =
        "GET / HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "User-Agent: suminworld-system-lab/1.0\r\n"
        "\r\n";
    char bufreq[512];
    snprintf(bufreq, sizeof(bufreq), req, req_host_hdr);

    ssize_t sent = send(sock, bufreq, strlen(bufreq), 0);
    if (sent < 0) { perror("send"); close(sock); return -1; }
    printf("[+] %zd bytes 전송\n", sent);

    // 수신 부분 개선
    printf("[*] 응답 수신 시작...\n");
    char buf[4096]; 
    ssize_t n; 
    size_t total = 0;
    int recv_count = 0;
    
    // 5초 read 타임아웃
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    while ((n = recv(sock, buf, sizeof(buf)-1, 0)) > 0) {
        buf[n] = '\0';  // 0 대신 '\0' (더 명확)
        printf("%s", buf);  // fputs 대신 printf (일관성)
        fflush(stdout);     // 실시간 출력 보장
        total += (size_t)n;
        recv_count++;
    }
    
    if (n == 0) {
        printf("\n[+] 서버가 정상 종료 (총 %zu bytes, %d번 수신)\n", total, recv_count);
    } else if (n < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN || errno == ETIMEDOUT) {
            printf("\n[!] 수신 타임아웃 (총 %zu bytes)\n", total);
        } else {
            printf("\n[!] 수신 오류: %s (총 %zu bytes)\n", strerror(errno), total);
        }
    }

    close(sock);
    return 0;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    // 1순위: neverssl.com (HTTP 평문)
    if (fetch_http("neverssl.com", "80", "neverssl.com") == 0) return 0;

    // 실패 시 2순위: example.com
    fprintf(stderr, "[!] neverssl 실패 → example.com 재시도\n");
    return fetch_http("example.com", "80", "example.com");
}
