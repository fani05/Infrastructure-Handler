#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#define PID_FILE ".monitor_pid"

int write_pid_file(void) {
    int fd = open(PID_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("[EROARE] Nu pot crea .monitor_pid");
        return -1;
    }

    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d\n", (int)getpid());
    write(fd, buf, len);
    close(fd);

    printf("[INFO] PID %d scris in %s\n", (int)getpid(), PID_FILE);
    return 0;
}

void remove_pid_file(void) {
    if (unlink(PID_FILE) == 0)
        printf("[INFO] %s sters.\n", PID_FILE);
    else
        perror("[WARN] Nu am putut sterge .monitor_pid");
}

int main(void) {
    printf("=== monitor_reports pornit (PID: %d) ===\n", (int)getpid());
    printf("[INFO] Oprire cu Ctrl+C (SIGINT) — de implementat saptamana viitoare.\n\n");
    if (write_pid_file() == -1)
        return 1;
    printf("[INFO] Monitor activ. Astept evenimente...\n");
    while (1) {
        sleep(1);
    }
    remove_pid_file();
    printf("=== monitor_reports oprit ===\n");

    return 0;
}