/*
 * Signal Handling Demo
 * 
 * Demonstrates proper signal handling with async-signal-safety
 * 
 * Compile: gcc -O2 -Wall -Wextra -o signal_demo signal_demo.c
 * Run:     ./signal_demo
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>

/* Signal-safe global flags */
static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t paused = 0;
static volatile sig_atomic_t alarm_count = 0;

/* Helper: Convert unsigned int to decimal string (signal-safe) */
static void u32_to_dec(unsigned int x, char *buf, size_t *len) {
    char tmp[16];
    int i = 0;
    
    if (x == 0) {
        buf[0] = '0';
        *len = 1;
        return;
    }
    
    while (x && i < 16) {
        tmp[i++] = '0' + (x % 10);
        x /= 10;
    }
    
    for (int j = 0; j < i; j++) {
        buf[j] = tmp[i - 1 - j];
    }
    *len = (size_t)i;
}

/* Signal Handlers - All using signal-safe functions only */

void sigint_handler(int sig) {
    (void)sig;
    const char msg[] = "\nReceived SIGINT. Exit? (y/n): ";
    (void)write(1, msg, sizeof(msg) - 1);
    
    char c;
    if (read(0, &c, 1) > 0 && (c == 'y' || c == 'Y')) {
        (void)write(1, "\nExiting program.\n", 18);
        _exit(0);
    }
    (void)write(1, "Continuing...\n", 14);
}

void sigtstp_handler(int sig) {
    (void)sig;
    /* SIGTSTP is caught to demonstrate job control awareness,
     * but we prevent actual stopping by not calling default handler */
    const char msg[] = "\nReceived SIGTSTP. Stop prevented (demo).\n";
    (void)write(1, msg, sizeof(msg) - 1);
}

void sigalrm_handler(int sig) {
    (void)sig;
    alarm_count++;

    const char p1[] = "\nAlarm #";
    const char p2[] = " triggered!\n";

    /* 1) Prefix output */
    (void)write(1, p1, sizeof(p1) - 1);

    /* 2) Number conversion and output */
    char digits[16];
    size_t dlen = 0;
    u32_to_dec((unsigned)alarm_count, digits, &dlen);
    if (dlen > 0) (void)write(1, digits, dlen);

    /* 3) Suffix output */
    (void)write(1, p2, sizeof(p2) - 1);

    alarm(3);  /* alarm() is async-signal-safe */
}

void sigusr1_handler(int sig) {
    (void)sig;
    paused = !paused;
    if (paused) {
        (void)write(1, "\nPaused.\n", 9);
    } else {
        (void)write(1, "\nResumed.\n", 10);
    }
}

void sigusr2_handler(int sig) {
    (void)sig;
    alarm_count = 0;
    const char msg[] = "\nCounter reset.\n";
    (void)write(1, msg, sizeof(msg) - 1);
}

/* Install signal handler with sigaction (preferred) */
static void install_handler(int signo, void (*fn)(int)) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = fn;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  /* Auto-restart syscalls */
    sigaction(signo, &sa, NULL);
}

/* Demo Functions */

void demo_basic() {
    printf("\n=== Demo 1: Basic Signal Handling ===\n");
    printf("Press Ctrl+C to test SIGINT\n");
    printf("Press Ctrl+Z to test SIGTSTP (will be caught)\n");
    printf("Press 'q' to return to menu\n\n");
    
    /* Advanced: For race-free signal handling, consider pselect() with signal mask
     * instead of select(). pselect() atomically blocks signals during the wait.
     * Example: pselect(nfds, &readfds, NULL, NULL, &timeout, &sigmask); */
    
    int count = 0;
    while(1) {
        printf("Running... [%d]\n", count++);
        sleep(1);
        
        fd_set fds;
        struct timeval tv = {0, 0};
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        
        if (select(1, &fds, NULL, NULL, &tv) > 0) {
            char c;
            if (read(0, &c, 1) > 0 && c == 'q') {
                printf("Returning to menu...\n");
                return;
            }
        }
    }
}

void demo_mask() {
    printf("\n=== Demo 2: Signal Masking ===\n");
    sigset_t mask, oldmask;
    
    printf("\nPhase 1: SIGINT enabled (3 seconds)\n");
    for (int i = 3; i > 0; i--) {
        printf("  %d... (Ctrl+C works)\n", i);
        sleep(1);
    }
    
    printf("\nPhase 2: SIGINT blocked (5 seconds)\n");
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    
    for (int i = 5; i > 0; i--) {
        printf("  %d... (Ctrl+C blocked)\n", i);
        sleep(1);
    }
    
    printf("\nPhase 3: SIGINT restored\n");
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    printf("Ctrl+C works again!\n\n");
    
    sleep(2);
}

void demo_alarm() {
    printf("\n=== Demo 3: Alarm Signal ===\n");
    printf("Alarm triggers every 3 seconds\n");
    printf("Press 'q' to quit\n\n");
    
    alarm_count = 0;
    alarm(3);
    
    while(1) {
        sleep(1);
        
        fd_set fds;
        struct timeval tv = {0, 0};
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        
        if (select(1, &fds, NULL, NULL, &tv) > 0) {
            char c;
            if (read(0, &c, 1) > 0 && c == 'q') {
                alarm(0);
                printf("\nReturning to menu...\n");
                return;
            }
        }
    }
}

void demo_user_signals() {
    printf("\n=== Demo 4: User-Defined Signals ===\n");
    printf("From another terminal, run:\n");
    printf("  kill -USR1 %d  (pause/resume)\n", getpid());
    printf("  kill -USR2 %d  (reset counter)\n\n", getpid());
    printf("Press 'q' to quit\n\n");
    
    int count = 0;
    while(1) {
        if (!paused) {
            printf("Working... [%d]\n", count++);
        } else {
            printf("Paused... (send USR1 to resume)\n");
        }
        sleep(1);
        
        fd_set fds;
        struct timeval tv = {0, 0};
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        
        if (select(1, &fds, NULL, NULL, &tv) > 0) {
            char c;
            if (read(0, &c, 1) > 0 && c == 'q') {
                printf("\nReturning to menu...\n");
                return;
            }
        }
    }
}

void demo_job_control() {
    printf("\n=== Demo 5: Job Control ===\n");
    printf("PID: %d\n\n", getpid());
    printf("Job control demonstration:\n");
    printf("  Ctrl+Z is caught (won't actually stop)\n");
    printf("  This shows signal awareness without stopping\n\n");
    printf("Background/foreground:\n");
    printf("  $ ./signal_demo &  - Run in background\n");
    printf("  $ jobs             - List jobs\n");
    printf("  $ fg               - Bring to foreground\n\n");
    printf("Press 'q' to quit\n\n");
    
    int count = 0;
    while(1) {
        printf("[Job] Running... %d\n", count++);
        sleep(2);
        
        fd_set fds;
        struct timeval tv = {0, 0};
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        
        if (select(1, &fds, NULL, NULL, &tv) > 0) {
            char c;
            if (read(0, &c, 1) > 0 && c == 'q') {
                printf("\nReturning to menu...\n");
                return;
            }
        }
    }
}

void demo_simple() {
    printf("\n=== Demo 6: Simple pause() Loop ===\n");
    printf("Minimal signal handler with pause()\n");
    printf("Press Ctrl+C to exit\n\n");
    
    while(1) {
        pause();  /* Wait for any signal */
    }
}

void auto_test() {
    printf("\n=== Demo 7: Automated Test ===\n");
    printf("PID: %d\n\n", getpid());
    
    printf("Test sequence:\n");
    printf("  5s  - Send USR1 (pause)\n");
    printf("  8s  - Send USR1 (resume)\n");
    printf("  10s - Send USR2 (reset)\n");
    printf("  15s - End test\n\n");
    
    pid_t pid = getpid();
    pid_t child = fork();
    
    if (child == 0) {
        /* Child: send signals */
        sleep(5);
        printf("\n-> Auto: Sending USR1 (pause)\n");
        kill(pid, SIGUSR1);
        
        sleep(3);
        printf("\n-> Auto: Sending USR1 (resume)\n");
        kill(pid, SIGUSR1);
        
        sleep(2);
        printf("\n-> Auto: Sending USR2 (reset)\n");
        kill(pid, SIGUSR2);
        
        _exit(0);
    }
    
    /* Parent: do work */
    int count = 0;
    for (int i = 0; i < 15; i++) {
        if (!paused) {
            printf("Working... [%d] (alarms:%d)\n", count++, (int)alarm_count);
        } else {
            printf("Paused...\n");
        }
        sleep(1);
    }
    
    /* Clean up zombie */
    int status;
    waitpid(child, &status, 0);
    
    printf("\nTest completed!\n");
    sleep(2);
}

void show_menu() {
    printf("\n");
    printf("========================================\n");
    printf("        Signal Handling Demo\n");
    printf("========================================\n");
    printf("  1. Basic Signals (SIGINT, SIGTSTP)\n");
    printf("  2. Signal Masking\n");
    printf("  3. Alarm Signal\n");
    printf("  4. User Signals (USR1, USR2)\n");
    printf("  5. Job Control\n");
    printf("  6. Simple Handler (pause loop)\n");
    printf("  7. Automated Test\n");
    printf("  0. Exit\n");
    printf("========================================\n");
    printf("\nChoice (0-7): ");
}

int main(void) {
    /* Disable stdout buffering for cleaner output with signals */
    if (isatty(1)) {
        setvbuf(stdout, NULL, _IONBF, 0);
    }
    
    /* Install all handlers using sigaction */
    install_handler(SIGINT, sigint_handler);
    install_handler(SIGTSTP, sigtstp_handler);
    /* Experimental: To see default SIGTSTP behavior (actual job stop):
     * install_handler(SIGTSTP, SIG_DFL);
     * This shows the difference between catching vs default handling */
    install_handler(SIGALRM, sigalrm_handler);
    install_handler(SIGUSR1, sigusr1_handler);
    install_handler(SIGUSR2, sigusr2_handler);
    
    printf("\n");
    printf("========================================\n");
    printf("    Signal Handling Demo v1.0\n");
    printf("========================================\n");
    printf("\nPID: %d\n", getpid());
    
    while(1) {
        show_menu();
        
        int choice;
        if (scanf("%d", &choice) != 1) {
            int ch;
            if (feof(stdin)) {
                printf("\nEOF detected. Exiting.\n\n");
                return 0;  // 파이프 입력 끝나면 정상 종료
            }
            printf("\nInvalid input.\n");
            while ((ch = getchar()) != '\n' && ch != EOF) { /* discard */ }
            continue;
        }

{
    int ch2 = getchar();
    if (ch2 != '\n' && ch2 != EOF) {
        while ((ch2 = getchar()) != '\n' && ch2 != EOF) {}
    }
}
        
        switch(choice) {
            case 1: demo_basic(); break;
            case 2: demo_mask(); break;
            case 3: demo_alarm(); break;
            case 4: demo_user_signals(); break;
            case 5: demo_job_control(); break;
            case 6: demo_simple(); break;
            case 7: auto_test(); break;
            case 0:
                printf("\nExiting program.\n\n");
                return 0;
            default:
                printf("\nInvalid choice.\n");
        }
    }
    
    return 0;
}