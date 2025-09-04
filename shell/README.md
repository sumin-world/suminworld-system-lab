# Mini Shell 전체 구조 도식화

## 1. 전체 아키텍처 개요

```
사용자 입력: "ls -l | grep txt > output.txt"
     ↓
┌─────────────────────────────────────────────────────────────┐
│                    Main Shell Loop                           │
│  while(1) { prompt → input → tokenize → parse → execute }   │
└─────────────────────────────────────────────────────────────┘
     ↓
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│   Tokenizer     │→ │     Parser      │→ │    Executor     │
│ 문자열 → 토큰배열 │  │ 토큰 → 구조체    │  │ 구조체 → 프로세스 │
└─────────────────┘  └─────────────────┘  └─────────────────┘
```

## 2. 데이터 변환 흐름

### 단계별 데이터 형태 변화
```
Step 1: 입력
"ls -l | grep txt > output.txt"

Step 2: Tokenize (문자열 → 배열)
["ls", "-l", "|", "grep", "txt", ">", "output.txt", NULL]

Step 3: Parse (배열 → 구조체)
Pipeline {
  cmds[0]: Command("ls", ["-l"])
  cmds[1]: Command("grep", ["txt"]) → output: "output.txt"
  ncmds: 2
  background: 0
}

Step 4: Execute (구조체 → 프로세스)
Process 1: ls -l  ──pipe──→  Process 2: grep txt ──→ output.txt
```

## 3. 핵심 데이터 구조

### Command 구조체 (개별 명령)
```c
typedef struct {
    char *argv[MAX_TOKENS];  // ["ls", "-l", NULL]
    int   argc;              // 2
    char *in_file;           // 입력 리다이렉션 파일
    char *out_file;          // 출력 리다이렉션 파일
    int   out_append;        // >> 모드 여부
} Command;
```

### Pipeline 구조체 (명령 파이프라인)
```c
typedef struct {
    Command cmds[MAX_CMDS];  // 여러 명령들의 배열
    int ncmds;               // 파이프 단계 수
    int background;          // & 백그라운드 여부
} Pipeline;
```

## 4. 시스템 콜 사용 맵

### 프로세스 관리
- `fork()` → 새 프로세스 생성
- `execvp()` → 프로그램 교체
- `waitpid()` → 자식 프로세스 대기
- `setpgid()` → 프로세스 그룹 설정

### I/O 관리
- `pipe()` → 파이프 생성
- `dup2()` → 파일 디스크립터 복제
- `open()` → 파일 열기
- `close()` → 파일 디스크립터 닫기

### 시그널 관리
- `sigaction()` → 시그널 핸들러 등록
- `kill()` → 시그널 전송

## 5. 실행 흐름 (예시: ls | grep)

```
Shell Process (PID: 1000)
    │
    ├─ fork() → Child 1 (PID: 1001) 
    │              │
    │              ├─ dup2(pipe[1], STDOUT)
    │              ├─ close(pipe[0])
    │              └─ execvp("ls", ["ls"])
    │
    ├─ fork() → Child 2 (PID: 1002)
    │              │
    │              ├─ dup2(pipe[0], STDIN)
    │              ├─ close(pipe[1])
    │              └─ execvp("grep", ["grep", "txt"])
    │
    ├─ close(both pipe ends)
    └─ waitpid(1001), waitpid(1002)
```

## 6. 내장 명령 vs 외부 명령

### 내장 명령 (Built-in)
```
cd, pwd, exit
↓
부모 프로세스에서 직접 실행
(쉘의 상태를 변경해야 하기 때문)
```

### 외부 명령 (External)
```
ls, grep, cat, echo 등
↓
fork() + execvp()로 자식 프로세스에서 실행
```

## 7. 시그널 처리 구조

```
User presses Ctrl-C (SIGINT)
    ↓
Shell's SIGINT handler (sigint_handler)
    ↓
if (foreground process group exists)
    kill(-fg_pgid, SIGINT)  // 포그라운드 그룹에만 전달
else
    do nothing  // 쉘 자체는 죽지 않음
```

## 8. OSTEP 개념과의 연결

- **Chapter 5 (Process API)**: fork(), execvp(), waitpid()
- **Chapter 36 (I/O Redirection)**: pipe(), dup2(), open()
- **Chapter 39 (Signals)**: sigaction(), kill()
- **Chapter 5 (Process Groups)**: setpgid()