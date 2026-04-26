#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

//structura de date pentru rapoarte
typedef struct {
    int id;
    char inspector[50];
    double latitude;
    double longitude;
    char category[20];
    int severity;
    time_t timestamp;
    char description[100];
} Report;

//transformarea pentru afisare
void mode_to_string(mode_t mode, char *buf) {
    buf[0] = (mode & S_IRUSR) ? 'r' : '-';
    buf[1] = (mode & S_IWUSR) ? 'w' : '-';
    buf[2] = (mode & S_IXUSR) ? 'x' : '-';
    buf[3] = (mode & S_IRGRP) ? 'r' : '-';
    buf[4] = (mode & S_IWGRP) ? 'w' : '-';
    buf[5] = (mode & S_IXGRP) ? 'x' : '-';
    buf[6] = (mode & S_IROTH) ? 'r' : '-';
    buf[7] = (mode & S_IWOTH) ? 'w' : '-';
    buf[8] = (mode & S_IXOTH) ? 'x' : '-';
    buf[9] = '\0';
}

//verificarea de permisiuni pentru fiecare user care incearca sa faca o operatie
int check_permission(const char *path, const char *role, char operatie) {
    struct stat st;
    if (stat(path, &st) == -1) {
        perror("check_permission: stat");
        return 0;
    }
    mode_t mode = st.st_mode;

    if (strcmp(role, "manager") == 0) {
        if (operatie == 'r') return (mode & S_IRUSR) ? 1 : 0;
        if (operatie == 'w') return (mode & S_IWUSR) ? 1 : 0;
    } else if (strcmp(role, "inspector") == 0) {
        if (operatie == 'r') return (mode & S_IRGRP) ? 1 : 0;
        if (operatie == 'w') return (mode & S_IWGRP) ? 1 : 0;
    }
    return 0;
}

// Functia de log care scrie de fiecare data cand o actiune este efectuata in fisereul de log
void log_action(const char *district, const char *role,
                const char *user, const char *action) {
    char path[256];
    snprintf(path, sizeof(path), "%s/logged_district", district);

    int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1) return;

    char line[256];
    int len = snprintf(line, sizeof(line), "%ld\t%s\t%s\t%s\n",
                       (long)time(NULL), user, role, action);
    write(fd, line, len);
    close(fd);
}

//Functia de initializare a fiecarui district + setarea de permisiuni
int init_district(const char *district) {
    char path[256];
    if (mkdir(district, 0750) == -1 && errno != EEXIST) {
        perror("Eroare creare director");
        return -1;
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
    snprintf(path, sizeof(path), "%s/logged_district", district);
    fd = open(path, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd != -1) close(fd);
    chmod(path, 0644);

    return 0;
}

//Creeaza un shortcut pentru a ajunge mai usor la directorul pe care il cautam
int create_active_symlink(const char *district) {
    char link_name[256];
    char target[256];
    snprintf(link_name, sizeof(link_name), "active_reports-%s", district);
    snprintf(target,    sizeof(target),    "%s/reports.dat",    district);
    struct stat lst;
    if (lstat(link_name, &lst) == 0) {
        unlink(link_name);
    }

    if (symlink(target, link_name) == -1) {
        perror("symlink");
        return -1;
    }
    printf("[INFO] Symlink creat: %s -> %s\n", link_name, target);
    return 0;
}

//se asigura ca directorul tinta chiar exista inainte de a il accesa
void check_symlink(const char *district) {
    char link_name[256];
    snprintf(link_name, sizeof(link_name), "active_reports-%s", district);

    struct stat lst;
    if (lstat(link_name, &lst) == -1) {
        printf("[INFO] Symlink %s nu exista.\n", link_name);
        return;
    }
    if (!S_ISLNK(lst.st_mode)) {
        printf("[WARN] %s exista dar nu este symlink!\n", link_name);
        return;
    }
    struct stat st;
    if (stat(link_name, &st) == -1) {
        printf("[AVERTISMENT] Dangling symlink: %s (tinta lipseste)\n", link_name);
    } else {
        printf("[INFO] Symlink valid: %s (%lld bytes)\n",
               link_name, (long long)st.st_size);
    }
}

//functia de adaugare propriu zisa a unui raport nou intr-un district
int adauga_raport(const char *district, const char *role, const char *user) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    if (!check_permission(path, role, 'w')) {
        fprintf(stderr, "[EROARE] Rolul '%s' nu are acces de scriere.\n", role);
        return -1;
    }

    struct stat st;
    stat(path, &st);
    int next_id = (int)(st.st_size / sizeof(Report)) + 1;

    Report r;
    memset(&r, 0, sizeof(Report));
    r.id        = next_id;
    r.timestamp = time(NULL);
    strncpy(r.inspector, user, sizeof(r.inspector) - 1);

    printf("--- Adaugare Raport Nou (ID: %d) ---\n", next_id);
    printf("Latitudine:  "); scanf("%lf", &r.latitude);
    printf("Longitudine: "); scanf("%lf", &r.longitude);
    printf("Categorie (road/lighting/flooding/other): ");
    scanf("%19s", r.category);
    printf("Severitate (1=minor 2=moderat 3=critic): ");
    scanf("%d", &r.severity);
    printf("Descriere: ");
    getchar();
    fgets(r.description, sizeof(r.description), stdin);
    r.description[strcspn(r.description, "\n")] = '\0';

    int fd = open(path, O_WRONLY | O_APPEND);
    if (fd == -1) { perror("Eroare deschidere reports.dat"); return -1; }
    write(fd, &r, sizeof(Report));
    close(fd);
    chmod(path, 0664);

    log_action(district, role, user, "add");
    create_active_symlink(district);

    printf("[SUCCES] Raportul #%d salvat in %s\n", next_id, path);
    return 0;
}

// functia care listeaza toate infromatile prezente intr-un district
int list(const char *district, const char *role) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    if (!check_permission(path, role, 'r')) {
        fprintf(stderr, "[EROARE] Rolul '%s' nu are acces de citire.\n", role);
        return -1;
    }
    struct stat st;
    if (stat(path, &st) == -1) { perror("stat"); return -1; }

    char perm[10];
    mode_to_string(st.st_mode, perm);

    char timebuf[64];
    struct tm *tm_info = localtime(&st.st_mtime);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);

    long long total = (long long)(st.st_size / sizeof(Report));
    printf("=== %s ===\n", path);
    printf("Permisiuni: %s  |  Dimensiune: %lld bytes  |  Modificat: %s\n",
           perm, (long long)st.st_size, timebuf);
    printf("Total rapoarte: %lld\n\n", total);

    if (total == 0) {
        printf("(niciun raport in acest district)\n");
        return 0;
    }

    //citirea din fisier pentru fiecare raport si apoi afisarea
    int fd = open(path, O_RDONLY);
    if (fd == -1) { perror("open"); return -1; }

    Report r;
    while (read(fd, &r, sizeof(Report)) == (ssize_t)sizeof(Report)) {
        char ts[32];
        tm_info = localtime(&r.timestamp);
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M", tm_info);
        printf("[#%d] cat:%-12s sev:%d inspector:%-20s gps:(%.4f, %.4f) la %s\n",
               r.id, r.category, r.severity, r.inspector,
               r.latitude, r.longitude, ts);
    }
    close(fd);

    log_action(district, role, "—", "list");
    return 0;
}

//functia de afisare in functie de ID a unui raport
int view(const char *district, const char *role, int report_id) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    if (!check_permission(path, role, 'r')) {
        fprintf(stderr, "[EROARE] Rolul '%s' nu are acces de citire.\n", role);
        return -1;
    }

    int fd = open(path, O_RDONLY);
    if (fd == -1) { perror("open"); return -1; }

    Report r;
    int found = 0;
    while (read(fd, &r, sizeof(Report)) == (ssize_t)sizeof(Report)) {
        if (r.id == report_id) {
            found = 1;
            char ts[32];
            struct tm *tm_info = localtime(&r.timestamp);
            strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
            printf("========== Raport #%d ==========\n", r.id);
            printf("Inspector : %s\n",    r.inspector);
            printf("Categorie : %s\n",    r.category);
            printf("Severitate: %d\n",    r.severity);
            printf("GPS       : %.6f, %.6f\n", r.latitude, r.longitude);
            printf("Timestamp : %s\n",    ts);
            printf("Descriere : %s\n",    r.description);
            break;
        }
    }
    close(fd);

    if (!found) {
        fprintf(stderr, "[EROARE] Raportul #%d nu exista.\n", report_id);
        return -1;
    }

    log_action(district, role, "—", "view");
    return 0;
}

//functia de stergere a unui raport
int sterge_raport(const char *district, const char *role, int report_id) {
    if (strcmp(role, "manager") != 0) {
        fprintf(stderr, "[EROARE] Doar managerul poate sterge rapoarte.\n");
        return -1;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    int fd = open(path, O_RDWR);
    if (fd == -1) { perror("open"); return -1; }

    struct stat st;
    fstat(fd, &st);
    long total = (long)(st.st_size / sizeof(Report));
    long del_idx = -1;
    Report r;
    for (long i = 0; i < total; i++) {
        lseek(fd, i * (off_t)sizeof(Report), SEEK_SET);
        read(fd, &r, sizeof(Report));
        if (r.id == report_id) { del_idx = i; break; }
    }

    if (del_idx == -1) {
        fprintf(stderr, "[EROARE] Raportul #%d nu a fost gasit.\n", report_id);
        close(fd);
        return -1;
    }

    //Shiftarea dupa ce raportul a fost sters
    for (long i = del_idx + 1; i < total; i++) {
        lseek(fd, i * (off_t)sizeof(Report), SEEK_SET);
        read(fd, &r, sizeof(Report));
        lseek(fd, (i - 1) * (off_t)sizeof(Report), SEEK_SET);
        write(fd, &r, sizeof(Report));
    }
    ftruncate(fd, (total - 1) * (off_t)sizeof(Report));
    close(fd);

    printf("[SUCCES] Raportul #%d a fost sters din '%s'.\n", report_id, district);
    log_action(district, role, "—", "remove_report");
    return 0;
}

//Functia de schimbare a thersholdului pentru escaladare
int update_threshold(const char *district, const char *role, int value) {
    if (strcmp(role, "manager") != 0) {
        fprintf(stderr, "[EROARE] Doar managerul poate modifica threshold-ul.\n");
        return -1;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/district.cfg", district);
    struct stat st;
    if (stat(path, &st) == -1) { perror("stat"); return -1; }
    if ((st.st_mode & 0777) != 0640) {
        fprintf(stderr, "[EROARE] Permisiunile lui district.cfg nu sunt 640. Refuz.\n");
        return -1;
    }

    if (!check_permission(path, role, 'w')) {
        fprintf(stderr, "[EROARE] Rolul '%s' nu are acces de scriere.\n", role);
        return -1;
    }

    int fd = open(path, O_WRONLY | O_TRUNC);
    if (fd == -1) { perror("open district.cfg"); return -1; }

    char line[64];
    int len = snprintf(line, sizeof(line), "threshold=%d\n", value);
    write(fd, line, len);
    close(fd);

    printf("[SUCCES] Threshold actualizat la %d in %s\n", value, path);
    log_action(district, role, "—", "update_threshold");
    return 0;
}

//FUNCTII GENERATE CU AJUTORUL AI
int parse_condition(const char *input, char *field, char *op, char *value) {
    char tmp[128];
    strncpy(tmp, input, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char *p1 = strchr(tmp, ':');
    if (!p1) return -1;
    *p1 = '\0';
    strncpy(field, tmp, 32);

    char *p2 = strchr(p1 + 1, ':');
    if (!p2) return -1;
    *p2 = '\0';
    strncpy(op,    p1 + 1, 4);
    strncpy(value, p2 + 1, 64);
    return 0;
}

int match_condition(Report *r, const char *field,
                    const char *op, const char *value) {
    if (strcmp(field, "severity") == 0) {
        int v = atoi(value);
        if (strcmp(op, "==") == 0) return r->severity == v;
        if (strcmp(op, "!=") == 0) return r->severity != v;
        if (strcmp(op, ">")  == 0) return r->severity >  v;
        if (strcmp(op, ">=") == 0) return r->severity >= v;
        if (strcmp(op, "<")  == 0) return r->severity <  v;
        if (strcmp(op, "<=") == 0) return r->severity <= v;
    } else if (strcmp(field, "category") == 0) {
        int cmp = strcmp(r->category, value);
        if (strcmp(op, "==") == 0) return cmp == 0;
        if (strcmp(op, "!=") == 0) return cmp != 0;
    } else if (strcmp(field, "inspector") == 0) {
        int cmp = strcmp(r->inspector, value);
        if (strcmp(op, "==") == 0) return cmp == 0;
        if (strcmp(op, "!=") == 0) return cmp != 0;
    } else if (strcmp(field, "timestamp") == 0) {
        time_t v = (time_t)atol(value);
        if (strcmp(op, ">=") == 0) return r->timestamp >= v;
        if (strcmp(op, "<=") == 0) return r->timestamp <= v;
        if (strcmp(op, "==") == 0) return r->timestamp == v;
        if (strcmp(op, ">")  == 0) return r->timestamp >  v;
        if (strcmp(op, "<")  == 0) return r->timestamp <  v;
    }
    return 0;
}

int filter(const char *district, const char *role,
               int num_conds, char **cond_strs) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    if (!check_permission(path, role, 'r')) {
        fprintf(stderr, "[EROARE] Rolul '%s' nu are acces de citire.\n", role);
        return -1;
    }

    if (num_conds <= 0) {
        fprintf(stderr, "[EROARE] Specificati cel putin o conditie.\n");
        return -1;
    }

    int fd = open(path, O_RDONLY);
    if (fd == -1) { perror("open"); return -1; }

    Report r;
    int found = 0;
    while (read(fd, &r, sizeof(Report)) == (ssize_t)sizeof(Report)) {
        int all_match = 1;
        for (int i = 0; i < num_conds; i++) {
            char field[32], op[4], value[64];
            if (parse_condition(cond_strs[i], field, op, value) != 0) {
                fprintf(stderr, "[EROARE] Conditie invalida: %s\n", cond_strs[i]);
                all_match = 0;
                break;
            }
            if (!match_condition(&r, field, op, value)) {
                all_match = 0;
                break;
            }
        }
        if (all_match) {
            printf("[#%d] sev:%d cat:%-12s inspector:%-20s desc:%s\n",
                   r.id, r.severity, r.category, r.inspector, r.description);
            found++;
        }
    }
    close(fd);

    if (!found) printf("(niciun raport nu satisface conditiile)\n");
    else printf("\nTotal gasite: %d\n", found);

    log_action(district, role, "—", "filter");
    return 0;
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Utilizare:\n"
        "  %s --role <rol> --user <user> --add <district>\n"
        "  %s --role <rol> --user <user> --list <district>\n"
        "  %s --role <rol> --user <user> --view <district> <id>\n"
        "  %s --role <rol> --user <user> --remove_report <district> <id>\n"
        "  %s --role <rol> --user <user> --update_threshold <district> <val>\n"
        "  %s --role <rol> --user <user> --filter <district> <cond...>\n"
        "\nRoluri valide: manager, inspector\n",
        prog, prog, prog, prog, prog, prog);
}

int main(int argc, char *argv[]) {
    const char *role     = NULL;
    const char *user     = NULL;
    const char *command  = NULL;
    const char *district = NULL;
    int         extra    = 0;

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "--role")             && i+1 < argc) { role     = argv[++i]; }
        else if (!strcmp(argv[i], "--user")             && i+1 < argc) { user     = argv[++i]; }
        else if (!strcmp(argv[i], "--add")              && i+1 < argc) { command  = "add";       district = argv[++i]; }
        else if (!strcmp(argv[i], "--list")             && i+1 < argc) { command  = "list";      district = argv[++i]; }
        else if (!strcmp(argv[i], "--view")             && i+2 < argc) { command  = "view";      district = argv[++i]; extra = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--remove_report")    && i+2 < argc) { command  = "remove";    district = argv[++i]; extra = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--update_threshold") && i+2 < argc) { command  = "threshold"; district = argv[++i]; extra = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--filter")           && i+1 < argc) { command  = "filter";    district = argv[++i]; }
    }

    if (!role || !command || !district) { usage(argv[0]); return 1; }
    if (strcmp(role, "manager") != 0 && strcmp(role, "inspector") != 0) {
        fprintf(stderr, "[EROARE] Rol invalid: '%s'\n", role);
        return 1;
    }
    if (!user) user = "unknown";

    if (init_district(district) != 0) return 1;

    if      (!strcmp(command, "add"))       return adauga_raport(district, role, user);
    else if (!strcmp(command, "list"))      return list(district, role);
    else if (!strcmp(command, "view"))      return view(district, role, extra);
    else if (!strcmp(command, "remove"))    return sterge_raport(district, role, extra);
    else if (!strcmp(command, "threshold")) return update_threshold(district, role, extra);
    else if (!strcmp(command, "filter")) {
        int cond_start = 0;
        for (int i = 1; i < argc; i++)
            if (!strcmp(argv[i], "--filter")) { cond_start = i + 2; break; }
        return filter(district, role,
                          argc - cond_start,
                          cond_start < argc ? &argv[cond_start] : NULL);
    }

    return 0;
}