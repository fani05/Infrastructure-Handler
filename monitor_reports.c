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

static int running = 1;  

static void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

static void handle_sigterm(int sig) {
    (void)sig;
    running = 0;
}

static void handle_sigusr1(int sig) {
    (void)sig;
    const char *msg = "New report added.\n";
    write(STDOUT_FILENO, msg, strlen(msg));
}

static int write_pid_file(void) {
    int fd = open(PID_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { perror("Error: could not create monitor"); return -1; }
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d\n", (int)getpid());
    write(fd, buf, len);
    close(fd);
    return 0;
}

static void remove_pid_file(void) {
    if (unlink(PID_FILE) == -1 && errno != ENOENT)
        perror("Monitor could not be deleted");
}

static pid_t check_existing_monitor(void) {
    int fd = open(PID_FILE, O_RDONLY);
    if (fd == -1) return 0;

    char buf[32];
    memset(buf, 0, sizeof(buf));
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 0;

    pid_t existing = (pid_t)atoi(buf);
    if (existing <= 0) return 0;

    if (kill(existing, 0) == 0)
        return existing;

    return 0;
}

int main(void) {
    pid_t existing = check_existing_monitor();
    if (existing > 0) {
        printf("Error: there is another monitor running (PID %d).\n",
               (int)existing);
        fflush(stdout);
        return 1;
    }

    printf("monitor_reports STARTED (PID %d).\n", (int)getpid());
    fflush(stdout);

    if (write_pid_file() == -1) return 1;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT"); remove_pid_file(); return 1;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction SIGTERM"); remove_pid_file(); return 1;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigusr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("sigaction SIGUSR1"); remove_pid_file(); return 1;
    }

    printf("MONITOR: Waiting for signals.\n");
    fflush(stdout);

    while (running) {
        pause();
    }

    printf("MONITOR: STOPPED\n");
    fflush(stdout);
    remove_pid_file();

    return 0;
}