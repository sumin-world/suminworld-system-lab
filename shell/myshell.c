// myshell.c - a tiny UNIX-like shell
// features: execvp, pipelines, redirection (<, >, >>), background (&)
// builtins: cd, pwd, exit
// limitations: no quotes/escaping; tokens separated by whitespace only

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define MAX_LINE   4096
#define MAX_TOKENS 256
#define MAX_CMDS   32

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

static volatile pid_t fg_pgid = 0;

static void sigint_handler(int sig) {
    // 전달: 포그라운드 파이프라인이 실행 중이면 그 그룹에 SIGINT
    if (fg_pgid > 0) {
        kill(-fg_pgid, SIGINT);
    }
}

static void trim_newline(char *s) {
    size_t n = strlen(s);
    if (n && (s[n-1] == '\n')) s[n-1] = '\0';
}

static int is_special(const char *t) {
    return (!strcmp(t, "|") || !strcmp(t, "<") || !strcmp(t, ">") || !strcmp(t, ">>") || !strcmp(t, "&"));
}

static int tokenize(char *line, char *tokens[], int max_tokens) {
    int ntok = 0;
    char *p = strtok(line, " \t");
    while (p && ntok < max_tokens-1) {
        tokens[ntok++] = p;
        p = strtok(NULL, " \t");
    }
    tokens[ntok] = NULL;
    return ntok;
}

static int parse_pipeline(char *tokens[], int ntok, Pipeline *pl) {
    memset(pl, 0, sizeof(*pl));
    if (ntok == 0) return 0;

    // background: trailing &
    if (ntok > 0 && !strcmp(tokens[ntok-1], "&")) {
        pl->background = 1;
        tokens[--ntok] = NULL;
    }

    pl->ncmds = 1;
    int ci = 0; // current command index
    Command *cur = &pl->cmds[ci];

    for (int i = 0; i < ntok; i++) {
        char *t = tokens[i];

        if (!strcmp(t, "|")) {
            if (cur->argc == 0) { fprintf(stderr, "syntax error near '|'\n"); return -1; }
            pl->ncmds++;
            if (pl->ncmds > MAX_CMDS) { fprintf(stderr, "too many pipeline stages\n"); return -1; }
            ci++;
            cur = &pl->cmds[ci];
            continue;
        }

        if (!strcmp(t, "<")) {
            if (i+1 >= ntok) { fprintf(stderr, "syntax error: expected file after '<'\n"); return -1; }
            cur->in_file = tokens[++i];
            continue;
        }
        if (!strcmp(t, ">") || !strcmp(t, ">>")) {
            if (i+1 >= ntok) { fprintf(stderr, "syntax error: expected file after '>'\n"); return -1; }
            cur->out_file = tokens[++i];
            cur->out_append = (t[1] == '>');
            continue;
        }

        // normal arg
        cur->argv[cur->argc++] = t;
    }

    for (int k = 0; k < pl->ncmds; k++) {
        if (pl->cmds[k].argc == 0) { fprintf(stderr, "empty command in pipeline\n"); return -1; }
        pl->cmds[k].argv[pl->cmds[k].argc] = NULL;
    }
    return 0;
}

static int is_builtin(Command *c) {
    if (c->argc == 0) return 0;
    return (!strcmp(c->argv[0], "cd") || !strcmp(c->argv[0], "pwd") || !strcmp(c->argv[0], "exit"));
}

static int run_builtin(Command *c) {
    if (!strcmp(c->argv[0], "cd")) {
        const char *target = (c->argc >= 2) ? c->argv[1] : getenv("HOME");
        if (!target) target = "/";
        if (chdir(target) != 0) perror("cd");
        return 0;
    } else if (!strcmp(c->argv[0], "pwd")) {
        char buf[4096];
        if (getcwd(buf, sizeof(buf))) {
            puts(buf);
        } else {
            perror("pwd");
        }
        return 0;
    } else if (!strcmp(c->argv[0], "exit")) {
        exit(0);
    }
    return -1;
}

static void exec_command(Command *c, int in_fd, int out_fd) {
    // I/O redirection
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

    // reset SIGINT to default in child so Ctrl-C kills foreground child
    signal(SIGINT, SIG_DFL);

    execvp(c->argv[0], c->argv);
    perror(c->argv[0]);
    _exit(127);
}

static int run_pipeline(Pipeline *pl) {
    // single builtin with no pipes: run in parent (so cd, pwd, exit work)
    if (pl->ncmds == 1 && is_builtin(&pl->cmds[0]) && !pl->background &&
        !pl->cmds[0].in_file && !pl->cmds[0].out_file) {
        return run_builtin(&pl->cmds[0]);
    }

    int pipes[MAX_CMDS-1][2];
    for (int i = 0; i < pl->ncmds-1; i++) {
        if (pipe(pipes[i]) < 0) { perror("pipe"); return -1; }
    }

    pid_t pids[MAX_CMDS] = {0};
    pid_t pgid = 0;

    for (int i = 0; i < pl->ncmds; i++) {
        int in_fd  = (i == 0) ? STDIN_FILENO  : pipes[i-1][0];
        int out_fd = (i == pl->ncmds-1) ? STDOUT_FILENO : pipes[i][1];

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return -1; }

        if (pid == 0) {
            // child
            // close unused pipe ends
            for (int k = 0; k < pl->ncmds-1; k++) {
                if (k != i-1) close(pipes[k][0]);
                if (k != i)   close(pipes[k][1]);
            }
            // set process group
            if (pgid) setpgid(0, pgid);
            else setpgid(0, 0);

            exec_command(&pl->cmds[i], in_fd, out_fd);
            // never returns
        } else {
            // parent
            pids[i] = pid;
            if (pgid == 0) pgid = pid;
            setpgid(pid, pgid);
            // close used pipe ends in parent
            if (i > 0) close(pipes[i-1][0]);
            if (i < pl->ncmds-1) close(pipes[i][1]);
        }
    }

    // foreground or background
    if (pl->background) {
        printf("[bg] started pgid=%d\n", (int)pgid);
        return 0;
    } else {
        // mark as foreground group for SIGINT forwarding
        fg_pgid = pgid;
        int status = 0;
        for (int i = 0; i < pl->ncmds; i++) {
            int st;
            if (waitpid(pids[i], &st, 0) > 0) status = st;
        }
        fg_pgid = 0;
        return status;
    }
}

int main(void) {
    // install SIGINT handler in shell
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    char line[MAX_LINE];

    while (1) {
        // prompt
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) {
            printf("myshell:%s$ ", cwd);
        } else {
            printf("myshell$ ");
        }
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            putchar('\n');
            break; // EOF (Ctrl-D)
        }
        trim_newline(line);
        if (line[0] == '\0') continue;

        // tokenize
        char *tokens[MAX_TOKENS];
        int ntok = tokenize(line, tokens, MAX_TOKENS);
        if (ntok == 0) continue;

        // parse
        Pipeline pl;
        if (parse_pipeline(tokens, ntok, &pl) != 0) {
            continue;
        }

        // run
        run_pipeline(&pl);
    }
    return 0;
}
