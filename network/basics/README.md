# TCP HTTP 클라이언트 (`/network/basics/tcp_client.c`)

🔗 **GitHub Repository**: https://github.com/sumin-world/suminworld-system-lab  
📝 **상세 설명**: https://velog.io/@suminworld/C%EB%A1%9C-HTTP-%ED%81%B4%EB%9D%BC%EC%9D%B4%EC%96%B8%ED%8A%B8-%EB%A7%8C%EB%93%A4%EA%B8%B0-%EB%94%94%EB%B2%84%EA%B9%85-non-blocking-connect-%ED%83%80%EC%9E%84%EC%95%84%EC%9B%83-Fallback-%ED%8C%A8%ED%82%B7-%EA%B4%80%EC%B0%B0

C 소켓 프로그래밍 학습을 위한 HTTP 클라이언트 구현체입니다. 기본적인 TCP 연결부터 타임아웃 처리, 에러 핸들링까지 네트워킹의 핵심 개념들을 다룹니다.

## ✨ 구현 기능

- **Non-blocking Connect**: 네트워크 지연으로 인한 블로킹 방지
- **Connection Timeout**: 3초 연결 타임아웃으로 무한 대기 방지  
- **Receive Timeout**: 5초 수신 타임아웃으로 응답 지연 처리
- **DNS Resolution**: `getaddrinfo()`를 사용한 현대적 DNS 해결
- **Automatic Fallback**: 주 서버 실패시 백업 서버로 자동 전환
- **실시간 디버깅**: 연결 과정과 데이터 수신 상태를 실시간 모니터링

## 🔧 핵심 기술

**연결 관리**
- `fcntl()`을 사용한 소켓 블로킹/논블로킹 모드 전환
- `select()`를 이용한 I/O 멀티플렉싱
- `getsockopt(SO_ERROR)`로 연결 상태 확인

**타임아웃 처리**
```c
// Connect timeout (3초)
connect_with_timeout(sock, ai->ai_addr, ai->ai_addrlen, 3000)

// Receive timeout (5초)  
struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
```

**Fallback 전략**
1. 1순위: `neverssl.com:80` (HTTP 평문 테스트용)
2. 2순위: `example.com:80` (표준 테스트 도메인)

## 🏃‍♂️ 실행 방법

```bash
# 컴파일
gcc -Wall -Wextra -Wpedantic -O2 -g -std=c99 tcp_client.c -o tcp_client
# 선택: 런타임 버그 헌팅
# gcc ... -fsanitize=address,undefined

# 실행
./tcp_client
```

**시스템/포팅 노트**: Linux/Ubuntu 기준. `MSG_NOSIGNAL` 사용 → macOS라면 `SO_NOSIGPIPE` 필요(현재 코드는 Linux OK).

## 🔄 동작 개요 (How it works)

getaddrinfo로 v4/v6 모두 수집 → non-blocking connect(+3s) → 성공한 소켓에 전체 전송(send_all) → SO_RCVTIMEO(+5s)로 수신 → 실패 시 fallback(host2)

## 📊 실행 결과 예시

```
[*] DNS: neverssl.com:80
[*] 시도 IP=52.84.228.90 ...
[+] 연결 성공 (52.84.228.90)
[+] 234 bytes 전송
[*] 응답 수신 시작...
HTTP/1.1 200 OK
Content-Type: text/html; charset=UTF-8
...
[+] 서버가 정상 종료 (총 3847 bytes, 12번 수신)
```

## 🛠 구현 세부사항

**에러 처리**
```
EINPROGRESS  : non-blocking 연결 진행 중(정상)
ETIMEDOUT    : 연결/수신 타임아웃
ECONNREFUSED : 포트 닫힘/방화벽 거부
EHOSTUNREACH : 라우팅/게이트웨이 문제
```

**메모리 관리**
- `freeaddrinfo()`로 DNS 결과 메모리 해제
- 소켓 자동 정리 (`close()`)

**성능 최적화**
- 4KB 수신 버퍼로 효율적 데이터 처리
- `fflush(stdout)`로 실시간 출력 보장
- Unbuffered stdout으로 디버깅 향상

**안전/범위 주의**: 평문 HTTP만 테스트용. 공개 서비스 대상 연속 호출은 빈도 제한 권장.

## 🔍 네트워크 디버깅 (옵션)

```bash
# 패킷 관찰
sudo tcpdump -i <iface> -nn -s0 -c 50 'host neverssl.com and tcp port 80'
# 인터페이스 이름은 환경마다 enp0s1, eth0 등 다를 수 있음
```

## 🎯 학습 포인트

이 코드를 통해 다음을 학습할 수 있습니다:

- 소켓 프로그래밍 기초부터 응용까지
- Non-blocking I/O와 이벤트 기반 프로그래밍  
- 네트워크 타임아웃과 에러 핸들링
- DNS resolution과 IP 주소 처리
- HTTP 프로토콜 기본 구현
- 시스템 호출 활용법