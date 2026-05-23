#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

static pid_t hub_mon_pid = -1;

static pid_t mon_child_pid = -1;

static void hub_mon_sigterm(int sig) {
    (void)sig;
    if (mon_child_pid > 0) {
        kill(mon_child_pid, SIGTERM);
        waitpid(mon_child_pid, NULL, 0);
    }
    exit(0);
}

static void run_hub_mon(void) {
    int pfd[2];
    int pid;

    if (pipe(pfd) < 0) {
        perror("Error creating the pipe");
        exit(1);
    }

    if ((pid = fork()) < 0) {
        perror("Error fork");
        exit(1);
    }

    if (pid == 0) {
        close(pfd[0]);
        dup2(pfd[1], 1);
        close(pfd[1]);
        execlp("./monitor_reports", "monitor_reports", NULL);
        perror("Error: monitor_reports");
        exit(1);
    }

    mon_child_pid = pid;
    close(pfd[1]);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = hub_mon_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);

    FILE *stream = fdopen(pfd[0], "r");
    if (stream == NULL) { perror("fdopen"); exit(1); }

    char line[512];
    while (fgets(line, sizeof(line), stream) != NULL) {
        line[strcspn(line, "\n")] = '\0';

        if (strncmp(line, "ERR:", 4) == 0) {
            printf("[hub_mon][EROARE] %s\n", line + 4);
        } else if (strncmp(line, "MSG:", 4) == 0) {
            printf("[hub_mon][NOTIFY] %s\n", line + 4);
        } else if (strncmp(line, "INFO:", 5) == 0) {
            printf("[hub_mon][info] %s\n", line + 5);
        } else if (line[0] != '\0') {
            printf("[hub_mon] %s\n", line);
        }
        fflush(stdout);
    }

    fclose(stream);
    wait(NULL);
    printf("Monitor finished.\n");
    fflush(stdout);
    exit(0);
}

static void cmd_start_monitor(void) {
    int pid;

    if (hub_mon_pid > 0 && kill(hub_mon_pid, 0) == 0) {
        printf("Another hub_mon already active (PID %d).\n", (int)hub_mon_pid);
        return;
    }

    if ((pid = fork()) < 0) {
        perror("Eroare fork");
        return;
    }

    if (pid == 0) {
        run_hub_mon();
        exit(0);
    }

    hub_mon_pid = pid;
    printf("hub_mon started (PID %d).\n", (int)pid);
}

static void cmd_calculate_scores(char *districts_line) {
    if (districts_line == NULL || districts_line[0] == '\0') {
        printf("USE: calculate_scores <district1> <district2> ...\n");
        return;
    }

    printf("\n========== RAPORT WORKLOAD COOMBINED ==========\n");

    char *district = strtok(districts_line, " \t");
    while (district != NULL) {
        int pfd[2];
        int pid;

        if (pipe(pfd) < 0) { perror("Error creating the pipe"); break; }

        if ((pid = fork()) < 0) {
            perror("Error fork");
            close(pfd[0]); close(pfd[1]);
            break;
        }

        if (pid == 0) {
            close(pfd[0]);
            dup2(pfd[1], 1);
            close(pfd[1]);
            execlp("./scorer", "scorer", district, NULL);
            perror("Error exec scorer");
            exit(1);
        }

        close(pfd[1]);

        FILE *stream = fdopen(pfd[0], "r");
        if (stream != NULL) {
            char line[512];
            while (fgets(line, sizeof(line), stream) != NULL)
                printf("%s", line);
            fclose(stream);
        }

        wait(NULL);
        district = strtok(NULL, " \t");
    }

    printf("==============================================\n\n");
}

static void stop_monitor(void) {
    if (hub_mon_pid > 0 && kill(hub_mon_pid, 0) == 0) {
        printf("[hub] Stopping hub_mon (PID %d)...\n", (int)hub_mon_pid);
        kill(hub_mon_pid, SIGTERM);
        wait(NULL);
        hub_mon_pid = -1;
    }
}

static void process_line(char *line) {
    line[strcspn(line, "\n")] = '\0';
    if (line[0] == '\0') return;

    char *cmd = strtok(line, " \t");
    if (cmd == NULL) return;

    if (strcmp(cmd, "start_monitor") == 0)      cmd_start_monitor();
    else if (strcmp(cmd, "calculate_scores") == 0) {
        char *rest = strtok(NULL, "");
        cmd_calculate_scores(rest);
    }
    else if (strcmp(cmd, "help") == 0) {
        printf("Commands:\n");
        printf("start_monitor\n");
        printf("calculate_scores <d1> <d2> ...\n");
        printf("help\n");
        printf("exit\n");
    }
    else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
        stop_monitor();
        exit(0);
    }
    else printf("[hub] Unknow command: '%s'. Type 'help'.\n", cmd);
}

int main(void) {
    printf("Write 'help' for a list of commands.\n");

    char line[512];
    while (1) {
        printf("hub> ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n[hub] EOF. Closing.\n");
            stop_monitor();
            break;
        }
        process_line(line);
    }
    return 0;
}