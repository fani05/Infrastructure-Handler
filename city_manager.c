#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

typedef struct {
    int    id;             
    char   inspector[50];   
    double latitude;      
    double longitude;
    char   category[20];    
    int    severity;        
    time_t timestamp;       
    char   description[100]; 
} Report;

int init_district(const char *district) {
    char path[256];
    if (mkdir(district, 0750) == -1 && errno != EEXIST) {
        perror("Eroare creare director"); return -1;
    }
    chmod(district, 0750);
    snprintf(path, sizeof(path), "%s/reports.dat", district);
    int fd = open(path, O_CREAT | O_WRONLY | O_APPEND, 0664);
    if (fd != -1) close(fd);
    chmod(path, 0664);

    snprintf(path, sizeof(path), "%s/district.cfg", district);
    if (access(path, F_OK) != 0) {
        fd = open(path, O_CREAT | O_WRONLY, 0640);
        if (fd != -1) {
            write(fd, "threshold=2\n", 12);
            close(fd);
        }
    }
    chmod(path, 0640);

    return 0;
}

int cmd_add(const char *district, const char *user) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);
    struct stat st;
    stat(path, &st);
    int next_id = (int)(st.st_size / sizeof(Report)) + 1;

    Report r;
    memset(&r, 0, sizeof(Report));
    r.id = next_id;
    r.timestamp = time(NULL);
    strncpy(r.inspector, user, sizeof(r.inspector) - 1);

    printf("--- Adaugare Raport Nou ---\n");
    printf("Latitudine: "); scanf("%lf", &r.latitude);
    printf("Longitudine: "); scanf("%lf", &r.longitude);
    printf("Categorie: "); scanf("%19s", r.category);
    printf("Severitate (1-3): "); scanf("%d", &r.severity);
    printf("Descriere scunda: ");
    getchar();
    fgets(r.description, sizeof(r.description), stdin);
    r.description[strcspn(r.description, "\n")] = '\0';
    int fd = open(path, O_WRONLY | O_APPEND);
    if (fd == -1) { perror("Eroare deschidere reports.dat"); return -1; }
    write(fd, &r, sizeof(Report));
    close(fd);

    printf("[SUCCES] Raportul cu ID %d a fost salvat in %s.\n", next_id, path);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Utilizare: %s --add <district> --user <nume>\n", argv[0]);
        return 1;
    }

    const char *district = argv[2];
    const char *user = argv[4];

    if (init_district(district) == 0) {
        cmd_add(district, user);
    }

    return 0;
}
