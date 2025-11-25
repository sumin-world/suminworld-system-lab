# ClamAV 실시간 보호 설정 (inotify)

## 🎯 실습 목표
파일 생성/수정 시 자동으로 악성코드를 탐지하고 격리하는 실시간 보호 시스템 구축

## 📚 핵심 개념

### inotify란?
- Linux 커널의 파일 시스템 모니터링 API
- 파일/디렉토리의 생성, 수정, 삭제를 실시간 감지
- 주요 시스템 콜:
  - `inotify_init()`: 인스턴스 생성
  - `inotify_add_watch()`: 감시 대상 등록
  - `inotify_rm_watch()`: 감시 해제

### 왜 필요한가?
```
주기적 검사 (10분마다):
파일 생성 → [최대 10분 대기] → 검사
⚠️ 악성코드가 이미 실행될 수 있음

실시간 감지 (inotify):
파일 생성 → [0.1초 내] → 검사 → 차단
✅ 악성코드 실행 전 차단!
```

---

## 🔧 설치

### inotify-tools 설치
```bash
sudo apt update
sudo apt install -y inotify-tools

# 설치 확인
which inotifywait
inotifywait --version
```

---

## 📝 기본 사용법

### 1. 수동 모니터링 테스트
```bash
# 테스트 디렉토리 생성
mkdir -p ~/test_monitor

# 파일 변경 감지 시작
inotifywait -m -r -e create,modify,delete ~/test_monitor
```

**옵션 설명:**
- `-m`: monitor 모드 (계속 실행)
- `-r`: recursive (하위 디렉토리 포함)
- `-e`: events (감시할 이벤트 종류)

### 2. 다른 터미널에서 테스트
```bash
cd ~/test_monitor
echo "test" > file1.txt
echo "modify" >> file1.txt
rm file1.txt
```

**출력 예시:**
```
/home/sumin/test_monitor/ CREATE file1.txt
/home/sumin/test_monitor/ MODIFY file1.txt
/home/sumin/test_monitor/ DELETE file1.txt
```

---

## 🛡️ 실시간 보호 스크립트

### 스크립트 내용
[realtime_monitor.sh](./realtime_monitor.sh)

### 주요 기능
1. **파일 변경 실시간 감지** (inotifywait)
2. **자동 악성코드 검사** (clamdscan)
3. **감염 파일 자동 격리** (quarantine)
4. **모든 이벤트 로그 기록**

### 스크립트 구조 분석

#### 변수 선언
```bash
WATCH_DIR="$1"              # 감시할 디렉토리 (첫 번째 인자)
LOG_FILE="$HOME/clamav_realtime.log"  # 로그 파일 경로
```

#### 파이프라인 핵심
```bash
inotifywait -m -r -e create,close_write "$WATCH_DIR" --format '%w%f' | while read FILE
```

**동작 흐름:**
```
inotifywait 출력 (/path/to/file)
    ↓ (파이프)
while read 루프 (한 줄씩 읽기)
    ↓
FILE 변수에 경로 저장
    ↓
검사 및 처리
```

#### ClamAV 검사
```bash
SCAN_RESULT=$(sudo clamdscan --fdpass --no-summary "$FILE" 2>&1)
```

- `--fdpass`: 파일 디스크립터 전달 (권한 문제 해결)
- `--no-summary`: 요약 정보 제외
- `2>&1`: 에러 메시지도 함께 캡처

#### 조건 판단
```bash
if echo "$SCAN_RESULT" | grep -q "FOUND"; then
    # 악성코드 발견 → 격리
else
    # 정상 파일
fi
```

---

## 🚀 실행 방법

### 1. 실행 권한 부여
```bash
chmod +x realtime_monitor.sh
```

### 2. 모니터링 시작
```bash
./realtime_monitor.sh ~/test_monitor
```

### 3. 테스트 (다른 터미널에서)

#### 정상 파일
```bash
cd ~/test_monitor
echo "Hello World" > clean.txt
```

**결과:**
```
[2025-11-25 18:37:52] Detected: /home/sumin/test_monitor/clean.txt
[2025-11-25 18:37:52] ✅ Clean: /home/sumin/test_monitor/clean.txt
```

#### EICAR 테스트 파일
```bash
wget https://secure.eicar.org/eicar.com -O eicar.com
```

**결과:**
```
[2025-11-25 18:42:53] Detected: /home/sumin/test_monitor/eicar.com
[2025-11-25 18:42:53] ⚠️  MALWARE DETECTED: /home/sumin/test_monitor/eicar.com
/home/sumin/test_monitor/eicar.com: Win.Test.EICAR_HDB-1 FOUND
[2025-11-25 18:42:53] 🔒 Quarantined to: /home/sumin/clamav_quarantine
```

---

## 📊 격리 확인

### 격리된 파일 확인
```bash
ls -lh ~/clamav_quarantine/
```

### 로그 파일 확인
```bash
cat ~/clamav_realtime.log
```

---

## 🎓 학습 정리

### bash 스크립트 핵심 개념

#### 1. 변수
```bash
VAR="value"     # 선언 (공백 없음!)
echo "$VAR"     # 사용
```

#### 2. 조건문
```bash
if [ -z "$VAR" ]; then
    echo "변수가 비어있음"
fi
```

**자주 쓰는 조건:**
- `-z "$VAR"`: 변수가 비어있나?
- `-f "$FILE"`: 파일이 존재하나?
- `-d "$DIR"`: 디렉토리가 존재하나?

#### 3. 명령어 치환
```bash
RESULT=$(date '+%Y-%m-%d %H:%M:%S')
```

#### 4. 파이프라인
```bash
명령1 | 명령2 | 명령3
```

#### 5. 리다이렉션
```bash
>          # 덮어쓰기
>>         # 추가
2>&1       # 에러를 출력으로 합치기
2>/dev/null  # 에러 메시지 숨기기
```

---

## 🔒 보안 고려사항

### 장점
- ✅ 실시간 탐지로 빠른 대응
- ✅ 자동 격리로 피해 최소화
- ✅ 로그 기록으로 추적 가능

### 한계
- ❌ CPU 사용량 증가 (파일 많을 시)
- ❌ 제로데이 공격 탐지 어려움
- ❌ 네트워크 공격 탐지 불가

### 실무 보안 전략
```
다층 방어 (Defense in Depth):
1. 예방: 방화벽 + 패치 관리
2. 탐지: ClamAV 실시간 보호 + EDR
3. 대응: 자동 격리 + 알림 시스템
4. 복구: 백업 + 인시던트 대응팀
```

---

## 📖 참고 자료
- [inotify man page](https://man7.org/linux/man-pages/man7/inotify.7.html)
- [ClamAV Documentation](https://docs.clamav.net/)
- [EICAR Test File](https://www.eicar.org/)
