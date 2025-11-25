# ClamAV 실습

## 📌 실습 환경
- **OS**: Ubuntu 24.04.3 LTS (ARM64)
- **메모리**: 9.7GB
- **ClamAV 버전**: 1.4.3

---

## 🎯 실습 목표
리눅스 환경에서 오픈소스 백신 ClamAV를 설치하고, 바이러스 검사를 수행하는 방법을 학습합니다.

---

## 📚 1단계: ClamAV 설치

### 명령어
```bash
# 패키지 목록 최신화
sudo apt update

# ClamAV 및 데몬 설치
sudo apt install -y clamav clamav-daemon

# 설치 확인
clamscan --version
freshclam --version
```

### 명령어 설명

#### `sudo apt update`
- `sudo`: **SuperUser DO** - 관리자 권한으로 실행
- `apt`: **Advanced Package Tool** - 우분투 패키지 관리자
- `update`: 설치 가능한 패키지 목록을 최신 상태로 업데이트 (실제 설치 X)

**왜 필요한가?**
- 패키지 저장소의 최신 버전 정보를 받아옴
- `apt install` 전에 항상 실행 권장

#### `sudo apt install -y clamav clamav-daemon`
- `install`: 패키지 설치
- `-y`: **yes** - 설치 확인 메시지에 자동으로 "예" 응답
- `clamav`: 기본 백신 프로그램 (clamscan, freshclam 포함)
- `clamav-daemon`: 백그라운드 데몬 (clamd, clamdscan 포함)

**설치되는 구성 요소:**
| 패키지 | 포함 프로그램 | 용도 |
|--------|--------------|------|
| `clamav` | `clamscan`, `freshclam` | 수동 검사, DB 업데이트 |
| `clamav-daemon` | `clamd`, `clamdscan` | 데몬 실행, 빠른 검사 |

---

## 📚 2단계: 바이러스 DB 업데이트 확인

### 명령어
```bash
# freshclam 데몬 상태 확인
sudo systemctl status clamav-freshclam
```

### 명령어 설명

#### `systemctl`
- **system control** - systemd 서비스 관리 명령어
- systemd: 리눅스의 서비스(백그라운드 프로그램) 관리자

**주요 명령어:**
```bash
systemctl status <서비스>    # 상태 확인
systemctl start <서비스>     # 시작
systemctl stop <서비스>      # 중지
systemctl restart <서비스>   # 재시작
systemctl enable <서비스>    # 부팅 시 자동 실행 설정
```

#### `clamav-freshclam` 서비스
- 바이러스 DB를 자동으로 업데이트하는 백그라운드 프로그램
- `/var/lib/clamav/` 디렉토리에 DB 파일 저장
  - `daily.cld`: 매일 업데이트되는 최신 시그니처
  - `main.cvd`: 메인 바이러스 DB
  - `bytecode.cvd`: 바이트코드 패턴

**우리 환경 결과:**
- 총 시그니처: **8,708,923개**
- DB 크기: 약 360MB (디스크)

---

## 📚 3단계: clamd 데몬 설정

### 명령어
```bash
# 실행 디렉토리 생성 및 권한 설정
sudo mkdir -p /run/clamav
sudo chown clamav:clamav /run/clamav

# 소켓 서비스 시작
sudo systemctl start clamav-daemon.socket

# clamd 데몬 시작
sudo systemctl start clamav-daemon

# 상태 확인
sudo systemctl status clamav-daemon

# 소켓 파일 확인
ls -l /run/clamav/
```

### 명령어 설명

#### `mkdir -p /run/clamav`
- `mkdir`: **make directory** - 디렉토리 생성
- `-p`: **parent** - 상위 디렉토리도 함께 생성 (이미 있으면 무시)

#### `chown clamav:clamav /run/clamav`
- `chown`: **change owner** - 파일/디렉토리 소유자 변경
- `clamav:clamav`: 소유자:그룹 (둘 다 clamav)

**왜 필요한가?**
- `clamd` 데몬은 `clamav` 사용자로 실행됨
- `/run/clamav/` 디렉토리에 소켓 파일을 생성해야 함
- 소유자가 맞지 않으면 권한 오류 발생

#### 소켓 파일이란?
```bash
ls -l /run/clamav/clamd.ctl
# srw-rw-rw- 1 clamav clamav 0 Nov 25 18:01 clamd.ctl
```

- `s`: **socket** - 소켓 파일 타입
- 프로세스 간 통신(IPC)을 위한 특수 파일
- 실제 데이터 저장 X, 통신 채널 역할

**소켓 통신 흐름:**
```
clamdscan 명령 실행
    ↓
clamd.ctl 소켓 연결
    ↓
clamd 데몬에 검사 요청
    ↓
결과 수신
```

---

## 📚 4단계: 검사 실행

### 명령어
```bash
# clamdscan으로 빠른 검사
time sudo clamdscan -i --fdpass ~/suminworld-system-lab

# clamscan으로 비교 검사
time sudo clamscan -r -i ~/suminworld-system-lab
```

### 명령어 설명

#### `time`
- 명령어 실행 시간 측정
- 출력 항목:
  - `real`: 실제 경과 시간 (벽시계 기준)
  - `user`: CPU가 사용자 모드에서 사용한 시간
  - `sys`: CPU가 커널 모드에서 사용한 시간

#### `clamdscan` 옵션
- `-i`: **infected only** - 감염된 파일만 출력
- `--fdpass`: **file descriptor pass** - 파일 디스크립터 전달
  - 권한 문제 해결
  - `clamdscan`이 파일을 열고 fd를 `clamd`에 전달

#### `clamscan` 옵션
- `-r`: **recursive** - 하위 디렉토리까지 재귀 검사
- `-i`: infected only

### 성능 비교 결과

| 도구 | 시간 | 특징 |
|------|------|------|
| `clamdscan` | 0.285초 | DB가 메모리에 상주 (빠름) |
| `clamscan` | 11.709초 | 매번 DB 로드 (느림) |

**속도 차이: 약 41배**

---

## 📚 5단계: EICAR 테스트 파일 탐지

### 명령어
```bash
# 테스트 디렉토리 생성
mkdir -p ~/test_malware
cd ~/test_malware

# EICAR 파일 생성
echo 'X5O!P%@AP[4\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*' > eicar.txt

# 검사
sudo clamdscan --fdpass eicar.txt

# 정리
rm -rf ~/test_malware
```

### EICAR란?
- **European Institute for Computer Antivirus Research**
- 백신 테스트용 표준 파일
- **실제 악성코드 아님** (안전함)
- 모든 백신이 탐지하도록 국제 표준으로 약속됨

### 결과
```
/home/sumin/test_malware/eicar.txt: Eicar-Signature FOUND
Infected files: 1
```

✅ **ClamAV가 정상 작동함을 확인!**

---

## 📚 6단계: 로그 저장

### 명령어
```bash
sudo clamdscan --fdpass ~/test_malware \
  | tee ~/clamav_test_$(date +%F_%H%M).log
```

### 명령어 설명

#### `date +%F_%H%M`
- 현재 날짜/시간 형식 지정
- `%F`: YYYY-MM-DD (2025-11-25)
- `%H%M`: 시분 (1815)
- 결과: `2025-11-25_1815`

#### `tee`
- 표준 출력을 화면과 파일에 동시 출력
- **파이프와 함께 사용**
```bash
명령어 | tee 파일명
```

**동작:**
1. 왼쪽 명령어 실행
2. 출력을 화면에 표시
3. 동시에 파일에도 저장

---

## 💾 메모리 사용량
```bash
ps aux | grep clamd
```

**결과:**
- `clamd` 메모리 사용: **1.30GB** (13.4%)
- 전체 메모리 9.7GB 중 적절한 수준
- 870만 개 시그니처가 메모리에 상주

---

## 🎓 학습 정리

### 핵심 개념

1. **데몬 (Daemon)**
   - 백그라운드에서 계속 실행되는 프로그램
   - 사용자 상호작용 없이 서비스 제공

2. **소켓 파일 (Socket)**
   - 프로세스 간 통신(IPC) 채널
   - `/run/clamav/clamd.ctl`

3. **파일 디스크립터 (File Descriptor)**
   - 열린 파일을 추적하는 정수 번호
   - `--fdpass`로 권한 문제 해결

4. **systemd**
   - 리눅스 서비스 관리 시스템
   - `systemctl` 명령어로 제어

### 실무 적용

**언제 clamscan을 쓸까?**
- 메모리 부족 환경
- 가끔씩만 검사
- 일회성 작업

**언제 clamdscan을 쓸까?**
- 서버 환경 (항상 실행)
- 빈번한 검사
- 실시간 보호 (다음 실습 주제!)

---

## 📖 참고 자료
- [ClamAV 공식 문서](https://docs.clamav.net/)
- [Ubuntu ClamAV 가이드](https://help.ubuntu.com/community/ClamAV)
