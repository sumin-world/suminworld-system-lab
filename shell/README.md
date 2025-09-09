# Tiny Shell - A UNIX-like Shell Implementation

A minimal but feature-rich shell implementation in C, demonstrating advanced system programming concepts like job control, process groups, signal handling, and robust parsing.

## ✨ Features

### Core Shell Features
- **Command Execution**: External programs via `execvp()`
- **Pipeline Support**: Multi-stage pipelines with `|`
- **I/O Redirection**: Input (`<`), output (`>`), append (`>>`)
- **Background Jobs**: Commands ending with `&`
- **Built-in Commands**: `cd`, `pwd`, `exit`, `jobs`, `fg`, `bg`
- **Job Control**: Full process group management with `Ctrl+Z`, `Ctrl+C`
- **Shell-style Variable Expansion**: `$VAR` and `$?` (exit status)
- **Quoted Parsing**: Single (`'`) and double (`"`) quotes, backslash escapes
- **Shell-style Comments**: Hash (`#`) and C++-style (`//`) inline comments
  (Comments are recognized only when they appear outside quotes.)

### Advanced Implementation Features
- **Background Notifications**: Async job completion alerts
- **Race-Safe Operations**: Retry mechanisms for `setpgid()` races
- **Signal-Safe Job Management**: Async-safe SIGCHLD handling
- **Status-Aware Prompt**: `myshell[exit_code]:/current/dir$`

## 🎬 Demo

### Terminal Output Example
```bash
$ ./myshell
myshell[0]:/home/user$ sleep 3 &
[1] 12345
myshell[0]:/home/user$ jobs
[1] Running   sleep 3
myshell[0]:/home/user$ echo "Hello" | tr a-z A-Z
HELLO

[1] Done      sleep 3
myshell[0]:/home/user$ false
myshell[1]:/home/user$ echo "Last exit code: $?"
Last exit code: 1
myshell[1]:/home/user$ 
```

### Execution Flow Visualization
```
Input: "ls -l | grep '\.txt$' > results.txt &"
  ↓
┌─────────────────────────────────────────────────┐
│ [Tokenizer] → ["ls", "-l", "|", "grep", ".txt", │
│                ">", "results.txt", "&"]         │
└─────────────────────────────────────────────────┘
  ↓
┌─────────────────────────────────────────────────┐
│ [Parser] → Pipeline {                           │
│   cmds[0]: ls -l                                │
│   cmds[1]: grep .txt → output: results.txt      │
│   background: true                              │
│ }                                               │
└─────────────────────────────────────────────────┘
  ↓
┌─────────────────────────────────────────────────┐
│ [Executor] → fork() → Process Group 12345       │
│   PID 12346: ls -l    ──pipe──→  PID 12347:     │
│   grep .txt ──→ results.txt                     │
└─────────────────────────────────────────────────┘
  ↓
myshell[0]:/home/user$ [1] 12345
                        ↑
                    Job notification
```

## 🚀 Quick Start

### Requirements
- **POSIX-like OS** (Linux/macOS)
- **GCC/Clang** with POSIX headers
- **Tested on**: Ubuntu 22.04 (gcc 11+), macOS 14 (clang 15+)

### Build
```bash
gcc -Wall -Wextra -O2 -o myshell myshell.c
```

### Run
```bash
./myshell
```

## 📋 Usage Examples

### Basic Commands
```bash
# Simple command
ls -la

# Pipeline
ps aux | grep myshell | head -5

# I/O Redirection
echo "Hello World" > output.txt
cat < input.txt
ls >> logfile.txt
```

### Job Control
```bash
# Background job
sleep 10 &
# Output: [1] 12345

# View jobs
jobs
# Output: [1] Running   sleep 10

# Suspend current job (Ctrl+Z)
sleep 100
^Z
# Output: [1] Stopped   sleep 100

# Resume in background
bg %1

# Resume in foreground  
fg %1
```

### Variables and Status
```bash
# Exit status tracking
false
echo "Last command failed with code: $?"
# Output: Last command failed with code: 1

# Environment variables
echo "Your home directory is: $HOME"
```

### Quoting and Comments
```bash
# Comments
echo "This works" # This is ignored
echo "Also works" // This is also ignored

# Quoting preserves spaces and special characters
echo "Message with # hash inside"
echo 'Single quotes: no $VAR expansion'
echo "Double quotes: $HOME expands"

# Escaping
echo "Quote: \"Hello World\""
```

## 🔧 Architecture Highlights

### Signal-Safe Job Management
- **Async-safe SIGCHLD handler**: Uses simple pid→pgid mapping instead of `getpgid()`
- **Deferred notifications**: Job completion messages printed safely at prompt time
- **Race-resistant `setpgid()`**: Automatic retry on `ESRCH`/`EPERM` errors

### Robust Parsing Engine
- **State-machine tokenizer**: Handles nested quotes, escapes, and special tokens
- **Comment filtering**: Both `#` and `//` styles supported (outside quotes only)
- **Variable expansion**: Respects quoting rules (no expansion in single quotes)
- **Error recovery**: Graceful handling of unmatched quotes

### Process Group Control
- **Terminal ownership**: Proper `tcsetpgrp()` management for job control
- **Signal isolation**: `SIGINT`/`SIGTSTP` forwarded to foreground process group only
- **Background safety**: `SIGPIPE` ignored to prevent pipeline failures

## 📁 Architecture Overview

### 1. 전체 아키텍처 개요

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

### 2. Data Transformation Pipeline
```
Step 1: Raw Input
"ls -l | grep txt > output.txt"

Step 2: Tokenize (string → array)
["ls", "-l", "|", "grep", "txt", ">", "output.txt", NULL]

Step 3: Parse (array → structure)
Pipeline {
  cmds[0]: Command("ls", ["-l"])
  cmds[1]: Command("grep", ["txt"]) → output: "output.txt"
  ncmds: 2
  background: 0
}

Step 4: Execute (structure → processes)
Process 1: ls -l  ──pipe──→  Process 2: grep txt ──→ output.txt
```

### 3. Core Data Structures
```c
typedef struct {
    char *argv[MAX_TOKENS];
    int   argc;
    char *in_file;
    char *out_file;
    int   out_append;
} Command;

typedef struct {
    Command cmds[MAX_CMDS];
    int ncmds;
    int background;
} Pipeline;
```

### 4. Process Execution Model
```
Shell Process (PID: 1000)
    │
    ├─ fork() → Child 1 (PID: 1001) 
    │              └─ execvp("ls", ["ls"])
    │
    ├─ fork() → Child 2 (PID: 1002)
    │              └─ execvp("grep", ["grep", "txt"])
    │
    └─ waitpid(1001), waitpid(1002)
```

### 5. Built-in vs External Commands
```
Built-in → 실행 즉시 부모 프로세스에서 처리 (cd, exit 등)
External → fork() + execvp()로 자식 프로세스 실행
```

### 6. Signal Handling
```
Ctrl-C → SIGINT → 현재 포그라운드 프로세스 그룹으로 전달
Ctrl-Z → SIGTSTP → 현재 프로세스 그룹을 STOP 상태로 전환
```

## 🚧 Current Limitations

- No `&&`, `||`, `;` operators
- No globbing (`*`, `?`, `[]`)
- No subshells (`$(...)`, backticks)
- No here-documents (`<<`)
- No advanced redirection (`>&`, `<&`)

## 🛣️ Roadmap

1. **Command operators** (`&&`, `||`, `;`)
2. **Globbing support**
3. **Command substitution**
4. **Here-docs and advanced I/O**
5. **RC file & customizable prompt**

## 📚 Educational Value & OSTEP Connections

### What You'll Learn
This project teaches essential systems programming concepts:
- **Process API**: `fork()`, `exec()`, `wait()`
- **I/O Redirection**: `pipe()`, `dup2()`
- **Signals**: `sigaction()`, async-safe handlers
- **Job Control**: `setpgid()`, `tcsetpgrp()`

## 🤝 Contributing

Feel free to fork, modify, and submit pull requests! This project is designed to be educational, so clarity and comments are valued over pure optimization.

## 📄 License

MIT License - Feel free to use this code for learning and teaching purposes.

---

**Made for education and exploration of systems programming concepts.**