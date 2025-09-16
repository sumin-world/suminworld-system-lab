// myshell.c - a tiny UNIX-like shell (job control + mini parser + status in prompt)
// features: execvp, pipelines, redirection (<, >, >>), background (&), basic job control (jobs/fg/bg)
// builtins: cd, pwd, exit, jobs, fg, bg
// improvements over previous version:
//   - Job control with process groups & terminal control (tcsetpgrp)
//   - SIGCHLD-driven job table (RUNNING/STOPPED/DONE), bg/fg resume with SIGCONT
//   - Mini tokenizer with quotes (' ") and backslash escapes, special tokens | < > >> &
//   - Variable expansion: $VAR and $? (inside double quotes or unquoted; not inside single quotes)
//   - Prompt shows last exit status: myshell[status]:/cwd$
//   - Background job completion notifications
//   - Race-safety: safe_setpgid() retries for ESRCH/EPERM
//   - strdup() failure handling; SIGPIPE ignored
// limitations: no command substitution, no globbing, no here-docs, no advanced expansion rules

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <termios.h>
#include <ctype.h>

#define MAX_LINE     8192
#define MAX_TOKENS   512
#define MAX_CMDS     64
#define MAX_JOBS     128
#define MAX_CMDLINE  4096

typedef struct {
    char *argv[MAX_TOKENS];
    int   argc;
    char *in_file;      // "<"
    char *out_file;     // ">" or ">>"
    int   out_append;   // 0: truncate, 1: append
} Command;

typedef struct {
    Command cmds[MAX_CMDS];
    int ncmds;          // number of pipeline stages
    int background;     // ends with &
} Pipeline;

typedef enum { JOB_UNUSED=0, JOB_RUNNING, JOB_STOPPED, JOB_DONE } job_state_t;

typedef struct {
    job_state_t state;
    pid_t pgid;
    char  cmdline[MAX_CMDLINE];
    int   notified;     // printed completion message
} Job;

// ---------------------------
// Globals
// ---------------------------
static volatile pid_t fg_pgid = 0;     // current foreground pgid (for SIGINT/TSTP)
static pid_t shell_pgid = 0;           // shell's process group id
static int   shell_terminal = -1;      // usually STDIN
static struct termios shell_tmodes;    // saved terminal modes
static Job jobs[MAX_JOBS];
static int last_status = 0;            // $?
static volatile sig_atomic_t pending_notifications = 0;

// Forward decl
static void print_jobs(void);
static int  parse_pipeline(char *tokens[], int ntok, Pipeline *pl);

// ---------------------------
// pid->pgid map (async-signal-safe alternative to getpgid() in handler)
// ---------------------------
typedef struct { pid_t pid; pid_t pgid; } PidMapEntry;
#define PIDMAP_CAP 4096
static volatile PidMapEntry pidmap[PIDMAP_CAP];
static volatile int pidmap_count = 0;

static inline void pidmap_add(pid_t pid, pid_t pgid) {
    int i = pidmap_count;
    if (i < PIDMAP_CAP) {
        pidmap[i].pid = pid;
        pidmap[i].pgid = pgid;
        pidmap_count = i + 1;
    }
}
static inline pid_t pidmap_get_pgid(pid_t pid) {
    for (int i = 0; i < pidmap_count; i++) if (pidmap[i].pid == pid) return pidmap[i].pgid;
    return -1;
}
static inline void pidmap_clear_pid(pid_t pid) {
    for (int i = 0; i < pidmap_count; i++) if (pidmap[i].pid == pid) { pidmap[i].pid = 0; pidmap[i].pgid = 0; return; }
}

// ---------------------------
// Job table helpers
// ---------------------------
static int add_job(pid_t pgid, const char *cmdline, job_state_t st) {
    for (int i = 1; i < MAX_JOBS; i++) {
        if (jobs[i].state == JOB_UNUSED || jobs[i].state == JOB_DONE) {
            jobs[i].state = st;
            jobs[i].pgid  = pgid;
            jobs[i].notified = 0;
            snprintf(jobs[i].cmdline, sizeof(jobs[i].cmdline), "%s", cmdline ? cmdline : "");
            return i;
        }
    }
    return -1;
}

static int find_job_by_pgid(pid_t pgid) {
    for (int i = 1; i < MAX_JOBS; i++) if (jobs[i].state && jobs[i].pgid == pgid) return i;
    return -1;
}

static void print_jobs(void) {
    for (int i = 1; i < MAX_JOBS; i++) {
        if (jobs[i].state == JOB_RUNNING) {
            printf("[%d] Running   %s\n", i, jobs[i].cmdline);
        } else if (jobs[i].state == JOB_STOPPED) {
            printf("[%d] Stopped   %s\n", i, jobs[i].cmdline);
        } else if (jobs[i].state == JOB_DONE) {
            printf("[%d] Done      %s\n", i, jobs[i].cmdline);
            jobs[i].state = JOB_UNUSED; // auto-clean after print
        }
    }
}

static void check_background_notifications(void) {
    if (!pending_notifications) return;
    pending_notifications = 0;
    for (int i = 1; i < MAX_JOBS; i++) {
        if (jobs[i].state == JOB_DONE && !jobs[i].notified) {
            printf("\n[%d] Done      %s\n", i, jobs[i].cmdline);
            jobs[i].state = JOB_UNUSED;
            jobs[i].notified = 1;
        }
    }
}

// ---------------------------
// Signals
// ---------------------------
static void sigint_handler(int sig) {
    (void)sig;
    if (fg_pgid > 0) kill(-fg_pgid, SIGINT);
}
static void sigtstp_handler(int sig) {
    (void)sig;
    if (fg_pgid > 0) kill(-fg_pgid, SIGTSTP);
}
static void sigchld_handler(int sig) {
    (void)sig;
    int saved = errno;
    while (1) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);
        if (pid <= 0) break;
        pid_t pg = pidmap_get_pgid(pid);
        if (pg < 0) continue;
        int jid = find_job_by_pgid(pg);
        if (WIFSTOPPED(status)) {
            if (jid > 0) jobs[jid].state = JOB_STOPPED;
        } else if (WIFCONTINUED(status)) {
            if (jid > 0) jobs[jid].state = JOB_RUNNING;
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            if (jid > 0) {
                jobs[jid].state = JOB_DONE;
                pending_notifications = 1;
            }
            pidmap_clear_pid(pid);
        }
    }
    errno = saved;
}

// ---------------------------
// String/Parsing
// ---------------------------
static void trim_newline(char *s) {
    size_t n = strlen(s);
    if (n && s[n-1]=='\n') s[n-1]='\0';
}

static size_t expand_vars(const char *in, char *out, size_t outsz) {
    size_t oi = 0;
    for (size_t i = 0; in[i] && oi + 1 < outsz; ) {
        if (in[i] == '$') {
            if (in[i+1] == '?') {
                char buf[32];
                snprintf(buf, sizeof(buf), "%d", last_status);
                size_t bl = strlen(buf);
                if (oi + bl >= outsz) bl = outsz - 1 - oi;
                memcpy(out + oi, buf, bl); oi += bl; i += 2; continue;
            }
            size_t j = i+1;
            if (!(isalpha((unsigned char)in[j])|| in[j]=='_')) { out[oi++] = in[i++]; continue; }
            size_t k = j; while (isalnum((unsigned char)in[k]) || in[k]=='_') k++;
            char name[256]; size_t nl = k - j; if (nl >= sizeof(name)) nl = sizeof(name) - 1;
            memcpy(name, in + j, nl); name[nl] = '\0';
            const char *val = getenv(name); if (!val) val = "";
            size_t vl = strlen(val); if (oi + vl >= outsz) vl = outsz - 1 - oi;
            memcpy(out + oi, val, vl); oi += vl; i = k; continue;
        }
        out[oi++] = in[i++];
    }
    out[oi] = '\0';
    return oi;
}

static int tokenize_quoted(const char *line, char *tokens[], int max_tokens) {
    int ntok = 0;
    const char *p = line;

    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (ntok >= max_tokens-1) break;

        /* ----- Handle line/inline comments (only outside quotes) ----- */
        /* If line starts with '#', treat rest of line as comment */
        if (*p == '#') {
            /* Skip to end-of-line */
            while (*p && *p != '\n') p++;
            continue;
        }
        /* Optional: C++-style comment (non-POSIX, convenience) */
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }

        char buf[MAX_LINE]; size_t bi = 0;
        int in_single = 0, in_double = 0;
        int unmatched_quote = 0;

        if (p[0]=='>' && p[1]=='>') {
            char *token = strdup(">>");
            if (!token) { fprintf(stderr, "Memory allocation failed\n"); return -1; }
            tokens[ntok++] = token; p += 2; continue;
        }
        if (*p=='|' || *p=='<' || *p=='>' || *p=='&') {
            char t[2] = { *p, '\0' };
            char *token = strdup(t);
            if (!token) { fprintf(stderr, "Memory allocation failed\n"); return -1; }
            tokens[ntok++] = token; p++; continue;
        }

        while (*p) {
            if (!in_single && !in_double && isspace((unsigned char)*p)) break;
            if (!in_single && !in_double && (*p=='|'||*p=='<'||*p=='>'||*p=='&')) break;

            /* ----- Handle inline comments while scanning a token ----- */
            /* If we encounter '#' outside quotes, the rest of the line is a comment */
            if (!in_single && !in_double && *p == '#') {
                /* Skip comment to EOL, then end current token */
                while (*p && *p != '\n') p++;
                break;
            }
            /* Optional: C++ style comments outside quotes */
            if (!in_single && !in_double && p[0] == '/' && p[1] == '/') {
                p += 2;
                while (*p && *p != '\n') p++;
                break;
            }

            if (*p == '\\') { p++; if (*p && bi < sizeof(buf)-1) buf[bi++] = *p++; continue; }
            if (*p == '\'') { if (!in_double) { in_single = !in_single; p++; continue; } }
            if (*p == '"')  { if (!in_single) { in_double = !in_double; p++; continue; } }

            if (bi < sizeof(buf)-1) buf[bi++] = *p++;
        }

        if (in_single || in_double) { fprintf(stderr, "Warning: unmatched quote in input\n"); unmatched_quote = 1; }
        buf[bi] = '\0';

        char expanded[MAX_LINE];
        if (in_single || unmatched_quote) {
            strncpy(expanded, buf, sizeof(expanded)); expanded[sizeof(expanded)-1]='\0';
        } else {
            expand_vars(buf, expanded, sizeof(expanded));
        }
        char *token = strdup(expanded);
        if (!token) { fprintf(stderr, "Memory allocation failed\n"); return -1; }
        tokens[ntok++] = token;
    }
    tokens[ntok] = NULL;
    return ntok;
}

static void free_tokens(char *tokens[], int ntok) {
    for (int i = 0; i < ntok; i++) free(tokens[i]);
}

// ---------------------------
// Parse into Pipeline
// ---------------------------
static int parse_pipeline(char *tokens[], int ntok, Pipeline *pl) {
    memset(pl, 0, sizeof(*pl));
    if (ntok == 0) return 0;

    if (ntok > 0 && !strcmp(tokens[ntok-1], "&")) {
        pl->background = 1;
        tokens[--ntok] = NULL;
    }

    pl->ncmds = 1;
    int ci = 0;
    Command *cur = &pl->cmds[ci];

    for (int i = 0; i < ntok; i++) {
        char *t = tokens[i];

        if (!strcmp(t, "|")) {
            if (cur->argc == 0) { fprintf(stderr, "syntax error near '|'\n"); return -1; }
            pl->ncmds++;
            if (pl->ncmds > MAX_CMDS) { fprintf(stderr, "too many pipeline stages\n"); return -1; }
            ci++; cur = &pl->cmds[ci]; continue;
        }
        if (!strcmp(t, "<")) {
            if (i+1 >= ntok) { fprintf(stderr, "syntax error: expected file after '<'\n"); return -1; }
            cur->in_file = tokens[++i]; continue;
        }
        if (!strcmp(t, ">") || !strcmp(t, ">>")) {
            if (i+1 >= ntok) { fprintf(stderr, "syntax error: expected file after '>'\n"); return -1; }
            cur->out_file = tokens[++i]; cur->out_append = (t[1] == '>'); continue;
        }
        cur->argv[cur->argc++] = t;
    }

    for (int k = 0; k < pl->ncmds; k++) {
        if (pl->cmds[k].argc == 0) { fprintf(stderr, "empty command in pipeline\n"); return -1; }
        pl->cmds[k].argv[pl->cmds[k].argc] = NULL;
    }
    return 0;
}

// ---------------------------
// Builtins
// ---------------------------
static int is_builtin_name(const char *s) {
    return (!strcmp(s, "cd") || !strcmp(s, "pwd") || !strcmp(s, "exit") ||
            !strcmp(s, "jobs") || !strcmp(s, "fg") || !strcmp(s, "bg"));
}
static int is_builtin(Command *c) { return c->argc>0 && is_builtin_name(c->argv[0]); }

static int builtin_cd(Command *c) {
    const char *target = (c->argc >= 2) ? c->argv[1] : getenv("HOME");
    if (!target) target = "/";
    if (chdir(target) != 0) { perror("cd"); return 1; }
    return 0;
}
static int builtin_pwd(Command *c) {
    (void)c;
    char buf[4096];
    if (getcwd(buf, sizeof(buf))) { puts(buf); return 0; }
    perror("pwd"); return 1;
}
static int builtin_exit(Command *c) {
    int code = (c->argc >= 2) ? atoi(c->argv[1]) : last_status;
    exit(code);
}
static int builtin_jobs(Command *c) { (void)c; print_jobs(); return 0; }

static int parse_job_id(const char *s) {
    if (!s) return -1;
    if (s[0] == '%') s++;
    for (const char *p=s; *p; ++p) if (!isdigit((unsigned char)*p)) return -1;
    long v = strtol(s, NULL, 10);
    if (v <= 0 || v >= MAX_JOBS) return -1;
    return (int)v;
}

static int builtin_fg(Command *c) {
    if (c->argc < 2) { fprintf(stderr, "fg %%jobid\n"); return 1; }
    int jid = parse_job_id(c->argv[1]); if (jid < 0 || !jobs[jid].state) { fprintf(stderr, "fg: no such job\n"); return 1; }
    pid_t pg = jobs[jid].pgid;
    if (tcsetpgrp(shell_terminal, pg) < 0) perror("tcsetpgrp");
    kill(-pg, SIGCONT);

    fg_pgid = pg;
    jobs[jid].state = JOB_RUNNING;

    int status = 0; pid_t w;
    do {
        w = waitpid(-pg, &status, WUNTRACED);
    } while (w > 0 && !WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));

    if (WIFSTOPPED(status)) jobs[jid].state = JOB_STOPPED; else jobs[jid].state = JOB_DONE;

    if (tcsetpgrp(shell_terminal, shell_pgid) < 0) perror("tcsetpgrp");
    fg_pgid = 0;

    if (WIFEXITED(status)) last_status = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) last_status = 128 + WTERMSIG(status);

    return 0;
}
static int builtin_bg(Command *c) {
    if (c->argc < 2) { fprintf(stderr, "bg %%jobid\n"); return 1; }
    int jid = parse_job_id(c->argv[1]); if (jid < 0 || !jobs[jid].state) { fprintf(stderr, "bg: no such job\n"); return 1; }
    kill(-jobs[jid].pgid, SIGCONT);
    jobs[jid].state = JOB_RUNNING;
    return 0;
}

static int run_builtin(Command *c) {
    if (!strcmp(c->argv[0], "cd"))   return builtin_cd(c);
    if (!strcmp(c->argv[0], "pwd"))  return builtin_pwd(c);
    if (!strcmp(c->argv[0], "exit")) return builtin_exit(c);
    if (!strcmp(c->argv[0], "jobs")) return builtin_jobs(c);
    if (!strcmp(c->argv[0], "fg"))   return builtin_fg(c);
    if (!strcmp(c->argv[0], "bg"))   return builtin_bg(c);
    return 127;
}

// ---------------------------
// I/O & exec helpers
// ---------------------------
static void setup_stdio(Command *c, int in_fd, int out_fd) {
    if (c->in_file) {
        int fd = open(c->in_file, O_RDONLY);
        if (fd < 0) { perror(c->in_file); _exit(127); }
        if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2 in"); _exit(127); }
        close(fd);
    } else if (in_fd != STDIN_FILENO) {
        if (dup2(in_fd, STDIN_FILENO) < 0) { perror("dup2 in_fd"); _exit(127); }
        close(in_fd);
    }

    if (c->out_file) {
        int flags = O_WRONLY | O_CREAT | (c->out_append ? O_APPEND : O_TRUNC);
        int fd = open(c->out_file, flags, 0644);
        if (fd < 0) { perror(c->out_file); _exit(127); }
        if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2 out"); _exit(127); }
        close(fd);
    } else if (out_fd != STDOUT_FILENO) {
        if (dup2(out_fd, STDOUT_FILENO) < 0) { perror("dup2 out_fd"); _exit(127); }
        close(out_fd);
    }
}
static void exec_external(Command *c, int in_fd, int out_fd) {
    setup_stdio(c, in_fd, out_fd);
    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    execvp(c->argv[0], c->argv);
    perror(c->argv[0]);
    _exit(127);
}

// setpgid with retry (race safety)
static void safe_setpgid(pid_t pid, pid_t pgid) {
    int retries = 3;
    while (retries-- > 0) {
        if (setpgid(pid, pgid) == 0) return;
        if (errno != ESRCH && errno != EPERM) break;
        usleep(1000);
    }
}

// ---------------------------
// Pipeline execution
// ---------------------------
static int run_pipeline(Pipeline *pl, const char *cmdline) {
    if (pl->ncmds == 1 && is_builtin(&pl->cmds[0]) && !pl->background &&
        !pl->cmds[0].in_file && !pl->cmds[0].out_file) {
        int rc = run_builtin(&pl->cmds[0]);
        last_status = rc;
        return rc;
    }

    int pipes_fd[MAX_CMDS-1][2];
    for (int i = 0; i < pl->ncmds-1; i++) if (pipe(pipes_fd[i]) < 0) { perror("pipe"); return -1; }
    pid_t pgid = 0;

    for (int i = 0; i < pl->ncmds; i++) {
        int in_fd  = (i == 0) ? STDIN_FILENO  : pipes_fd[i-1][0];
        int out_fd = (i == pl->ncmds-1) ? STDOUT_FILENO : pipes_fd[i][1];

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return -1; }

        if (pid == 0) {
            for (int k = 0; k < pl->ncmds-1; k++) {
                if (k != i-1) close(pipes_fd[k][0]);
                if (k != i)   close(pipes_fd[k][1]);
            }
            if (pgid) setpgid(0, pgid); else setpgid(0, 0);

            if (is_builtin(&pl->cmds[i])) {
                setup_stdio(&pl->cmds[i], in_fd, out_fd);
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                int rc = run_builtin(&pl->cmds[i]);
                _exit(rc);
            } else {
                exec_external(&pl->cmds[i], in_fd, out_fd);
            }
        } else {
            if (pgid == 0) pgid = pid;
            safe_setpgid(pid, pgid);
            pidmap_add(pid, pgid);
            if (i > 0) close(pipes_fd[i-1][0]);
            if (i < pl->ncmds-1) close(pipes_fd[i][1]);
        }
    }

    int jid = add_job(pgid, cmdline, JOB_RUNNING);

    if (pl->background) {
        printf("[%d] %d\n", jid, (int)pgid);
        return 0;
    } else {
        if (tcsetpgrp(shell_terminal, pgid) < 0) perror("tcsetpgrp");
        fg_pgid = pgid;

        int status = 0; pid_t w;
        do {
            w = waitpid(-pgid, &status, WUNTRACED);
        } while (w > 0 && !WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));

        if (tcsetpgrp(shell_terminal, shell_pgid) < 0) perror("tcsetpgrp");
        fg_pgid = 0;

        if (WIFSTOPPED(status)) {
            if (jid > 0) jobs[jid].state = JOB_STOPPED;
        } else {
            if (jid > 0) jobs[jid].state = JOB_DONE;
        }

        if (WIFEXITED(status)) last_status = WEXITSTATUS(status);
        else if (WIFSIGNALED(status)) last_status = 128 + WTERMSIG(status);
        else last_status = 0;

        return status;
    }
}

// ---------------------------
// Init & Main
// ---------------------------
static void init_shell(void) {
    shell_terminal = STDIN_FILENO;

    shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid); // 터미널 제어권을 내 그룹으로
    if (isatty(shell_terminal)) { // 키보드 모드라면 (tty=TeleTYpewriter)
        tcsetpgrp(shell_terminal, shell_pgid); // tcsetpgrp = tc(Terminal Control) + set + pgrp(Process GRouP) 터미널의 제어권을 이 프로세스 그룹에게 줌
        tcgetattr(shell_terminal, &shell_tmodes); // tcgetattr(터미널번호, 저장할곳), attr은 속성, 현재 터미널 설정들을 저장 
    }

    signal(SIGTTOU, SIG_IGN); // SIGTTOU 시그널 무시 (터미널 출력 관련)
    signal(SIGTTIN, SIG_IGN); // SIGTTIN 시그널 무시 (터미널 입력 관련)
    signal(SIGPIPE, SIG_IGN); // SIGPIPE 시그널 무시 (파이프 관련)

    struct sigaction sa_int = {0}, sa_tstp = {0}, sa_chld = {0};

    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, NULL);

    sa_tstp.sa_handler = sigtstp_handler;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_tstp, NULL);

    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa_chld, NULL);
}

static void print_prompt(void) {
    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd))) strcpy(cwd, "?");
    printf("myshell[%d]:%s$ ", last_status, cwd);
    fflush(stdout);
}

int main(void) {
    init_shell();
    char line[MAX_LINE];

    while (1) {
        check_background_notifications();
        print_prompt();

        if (!fgets(line, sizeof(line), stdin)) { putchar('\n'); break; }
        trim_newline(line);
        if (line[0] == '\0') continue;

        char *tokens[MAX_TOKENS];
        int ntok = tokenize_quoted(line, tokens, MAX_TOKENS);
        if (ntok < 0) continue;

        Pipeline pl; memset(&pl, 0, sizeof(pl));
        if (parse_pipeline(tokens, ntok, &pl) != 0) { free_tokens(tokens, ntok); continue; }

        char cmdline[MAX_CMDLINE] = {0};
        for (int i=0;i<ntok;i++) {
            strncat(cmdline, tokens[i], sizeof(cmdline)-1 - strlen(cmdline));
            if (i+1<ntok) strncat(cmdline, " ", sizeof(cmdline)-1 - strlen(cmdline));
        }

        (void)run_pipeline(&pl, cmdline);
        free_tokens(tokens, ntok);
    }
    return 0;
}
