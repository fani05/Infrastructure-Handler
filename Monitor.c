#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#define PID_FILE ".monitor_pid"
static volatile sig_atomic_t running = 1;

static void handle_sigint(int sig) {
    (void)sig; 
    running = 0;
}

static void handle_sigusr1(int sig) {
    (void)sig;
    const char *msg = "[MONITOR] Semnal primit: raport nou adaugat intr-un district.\n";
    write(STDOUT_FILENO, msg, strlen(msg));
}

static int write_pid_file(void) {
    int fd = open(PID_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("[EROARE] Nu pot crea .monitor_pid");
        return -1;
    }
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d\n", (int)getpid());
    write(fd, buf, len);
    close(fd);
    return 0;
}

static void remove_pid_file(void) {
    if (unlink(PID_FILE) == -1 && errno != ENOENT)
        perror("[WARN] Nu am putut sterge .monitor_pid");
}


int main(void) {
    printf("=== monitor_reports pornit (PID: %d) ===\n", (int)getpid());
    fflush(stdout);
    if (write_pid_file() == -1)
        return 1;
    printf("[INFO] PID %d scris in %s\n", (int)getpid(), PID_FILE);
    fflush(stdout);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; 
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT");
        remove_pid_file();
        return 1;
    }
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigusr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("sigaction SIGUSR1");
        remove_pid_file();
        return 1;
    }

    printf("[INFO] Astept semnale. Oprire cu Ctrl+C (SIGINT).\n");
    fflush(stdout);
    while (running) {
        pause();
    }
    printf("\n[MONITOR] SIGINT primit. Monitor oprit.\n");
    fflush(stdout);
    remove_pid_file();
    printf("[INFO] %s sters. La revedere.\n", PID_FILE);

    return 0;
}