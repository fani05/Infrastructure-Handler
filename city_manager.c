#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

//structura de date pentru rapoarte
//declarare fixa pentru a putea fi navigata cu lssek(stim ca dimensiunea este mereu de 202 bytes)
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
void determine_permissions(mode_t access_rights, char *buf) {
    buf[0] = (access_rights & S_IRUSR) ? 'r' : '-';
    buf[1] = (access_rights & S_IWUSR) ? 'w' : '-';
    buf[2] = (access_rights & S_IXUSR) ? 'x' : '-';
    buf[3] = (access_rights & S_IRGRP) ? 'r' : '-';
    buf[4] = (access_rights & S_IWGRP) ? 'w' : '-';
    buf[5] = (access_rights & S_IXGRP) ? 'x' : '-';
    buf[6] = (access_rights & S_IROTH) ? 'r' : '-';
    buf[7] = (access_rights & S_IWOTH) ? 'w' : '-';
    buf[8] = (access_rights & S_IXOTH) ? 'x' : '-';
    buf[9] = '\0';
}

//verificarea de permisiuni pentru fiecare user care incearca sa faca o operatie
int check_permission(const char *path, const char *role, char operatie) {
    struct stat st;
    if (stat(path, &st) == -1) {
        perror("check_permission: stat");
        return 0;
    }
    //mutam informatia care ne intereseaza intr-o variabila separata 
    mode_t access_rights = st.st_mode;

    if (strcmp(role, "manager") == 0) {
        if (operatie == 'r') return (access_rights & S_IRUSR) ? 1 : 0;
        if (operatie == 'w') return (access_rights & S_IWUSR) ? 1 : 0;
    } else if (strcmp(role, "inspector") == 0) {
        if (operatie == 'r') return (access_rights & S_IRGRP) ? 1 : 0;
        if (operatie == 'w') return (access_rights & S_IWGRP) ? 1 : 0;
    }
    return 0;
}

// Functia de log care scrie de fiecare data cand o actiune este efectuata in fisereul de log
void log_action(const char *district, const char *role,const char *user, const char *action) {
    //construim adresa pentru a deschide fisierul
    char path[256];
    snprintf(path, sizeof(path), "%s/logged_district", district);

    int fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1) return;

    char line[256];
    int len = snprintf(line, sizeof(line), "%ld\t%s\t%s\t%s\n",(long)time(NULL), user, role, action);
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
    printf("Symlink was created: %s -> %s\n", link_name, target);
    return 0;
}

//se asigura ca directorul tinta chiar exista inainte de a il accesa
void check_symlink(const char *district) {
    char link_name[256];
    snprintf(link_name, sizeof(link_name), "active_reports-%s", district);

    struct stat lst;
    if (lstat(link_name, &lst) == -1) {
        printf("Symlink %s does not exist\n", link_name);
        return;
    }
    if (!S_ISLNK(lst.st_mode)) {
        printf("%s is not a symlink\n", link_name);
        return;
    }

    struct stat st;
    if (stat(link_name, &st) == -1) {
        printf("Dangling symlink: %s (target is inexistent)\n", link_name);
    } else {
        printf("Symlink is valid: %s (%lld bytes)\n",link_name, (long long)st.st_size);
    }
}

//functia de notificare a monitorului
static int notify_monitor(void) {
    int fd = open(".monitor_pid", O_RDONLY);
    if (fd == -1) return 0;

    char buf[32];
    memset(buf, 0, sizeof(buf));
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 0;

    pid_t monitor_pid = (pid_t)atoi(buf);
    if (monitor_pid <= 0) return 0;

    if (kill(monitor_pid, SIGUSR1) == -1) return 0;
    return 1;
}

//functia de adaugare propriu zisa a unui raport nou intr-un district
int add_report(const char *district, const char *role, const char *user) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    if (!check_permission(path, role, 'w')) {
        fprintf(stderr, "Role: '%s' does not have permissions", role);
        return -1;
    }

    struct stat st;
    stat(path, &st);
    int next_id = (int)(st.st_size / sizeof(Report)) + 1;

    Report r;
    memset(&r, 0, sizeof(Report));
    r.id = next_id;
    r.timestamp = time(NULL);
    strncpy(r.inspector, user, sizeof(r.inspector) - 1);

    printf("Adding new report(ID: %d)\n", next_id);
    printf("Latitude:  "); scanf("%lf", &r.latitude);
    printf("Longitude: "); scanf("%lf", &r.longitude);
    printf("Category (road/lighting/flooding/other): ");
    scanf("%19s", r.category);
    printf("Severity (1=minor 2=moderat 3=critic): ");
    scanf("%d", &r.severity);
    printf("Descriere: ");
    getchar();
    fgets(r.description, sizeof(r.description), stdin);
    r.description[strcspn(r.description, "\n")] = '\0';

    int fd = open(path, O_WRONLY | O_APPEND);
    if (fd == -1) { perror("Error opening reports.dat"); return -1; }
    write(fd, &r, sizeof(Report));
    close(fd);
    chmod(path, 0664);

    log_action(district, role, user, "add");
    create_active_symlink(district);

    if (notify_monitor()) {
        log_action(district, role, user, "monitor_notified");
        printf("Monitor was notified\n");
    } else {
        log_action(district, role, user, "monitor_unavailable");
        printf("Monitor could not be notified\n");
    }

    printf("Report #%d was saved in %s\n", next_id, path);
    return 0;
}

// functia care listeaza toate infromatile prezente intr-un district
int list(const char *district, const char *role) {
    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    if (!check_permission(path, role, 'r')) {
        fprintf(stderr, "Role '%s' does not have reading access\n", role);
        return -1;
    }
    struct stat st;
    if (stat(path, &st) == -1) 
    { 
        perror("stat"); return -1;
    }

    char perm[10];
    determine_permissions(st.st_mode, perm);

    char timebuf[64];
    struct tm *tm_info = localtime(&st.st_mtime);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);

    long long total = (long long)(st.st_size / sizeof(Report));
    printf("=== %s ===\n", path);
    printf("Permissions %s  |  Size: %lld bytes  |  Modified: %s\n",
           perm, (long long)st.st_size, timebuf);
    printf("Total reports: %lld\n\n", total);

    if (total == 0) {
        printf("(No reports in this district)\n");
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
               r.id, r.category, r.severity, r.inspector,r.latitude, r.longitude, ts);
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
        fprintf(stderr, "Role '%s' does not have read access.\n", role);
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
            printf("========== Report #%d ==========\n", r.id);
            printf("Inspector: %s\n",r.inspector);
            printf("Category: %s\n",r.category);
            printf("Severity: %d\n",r.severity);
            printf("Location: %.6f, %.6f\n", r.latitude, r.longitude);
            printf("Timestamp: %s\n",ts);
            printf("Description: %s\n",r.description);
            break;
        }
    }
    close(fd);

    if (!found) {
        fprintf(stderr, "Report #%d does not exist.\n", report_id);
        return -1;
    }

    log_action(district, role, "—", "view");
    return 0;
}

//functia de stergere a unui raport
int delete_report(const char *district, const char *role, int report_id) {
    if (strcmp(role, "manager") != 0) {
        fprintf(stderr, "Only the manager can delete reports\n");
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
        fprintf(stderr, "Report #%d was not found.\n", report_id);
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

    printf("Report #%d was removed from: '%s'.\n", report_id, district);
    log_action(district, role, "—", "remove_report");
    return 0;
}

//Functia de schimbare a thersholdului pentru escaladare
int update_threshold(const char *district, const char *role, int value) {
    if (strcmp(role, "manager") != 0) {
        fprintf(stderr, "Only manager can modify threshold.\n");
        return -1;
    }

    char path[256];
    snprintf(path, sizeof(path), "%s/district.cfg", district);
    struct stat st;
    if (stat(path, &st) == -1) { perror("stat"); return -1; }
    if ((st.st_mode & 0777) != 0640) {
        fprintf(stderr, "Error: permissions do no match\n");
        return -1;
    }

    if (!check_permission(path, role, 'w')) {
        fprintf(stderr, "Role'%s' does not have writing access\n", role);
        return -1;
    }

    int fd = open(path, O_WRONLY | O_TRUNC);
    if (fd == -1) { perror("open district.cfg"); return -1; }

    char line[64];
    int len = snprintf(line, sizeof(line), "threshold=%d\n", value);
    write(fd, line, len);
    close(fd);

    printf("Threshold updated to %d in %s\n", value, path);
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
        fprintf(stderr, "Role '%s' does not have access.\n", role);
        return -1;
    }

    if (num_conds <= 0) {
        fprintf(stderr, "Error: no condition specified\n");
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
                fprintf(stderr, "Error: invalid condition:  %s\n", cond_strs[i]);
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

    if (!found) printf("No report matched the condition\n");
    else printf("\nTotal found reports: %d\n", found);

    log_action(district, role, "—", "filter");
    return 0;
}

int remove_district(const char *district, const char *role) {
    if (strcmp(role, "manager") != 0) {
        fprintf(stderr, "Only manager can remove a district.\n");
        return -1;
    }
 
    struct stat st;
    if (stat(district, &st) == -1) {
        fprintf(stderr, "District '%s' does not exist.\n", district);
        return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a directory.\n", district);
        return -1;
    }
 
    char link_name[256];
    snprintf(link_name, sizeof(link_name), "active_reports-%s", district);
    unlink(link_name);
 
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }
 
    if (pid == 0) {
        execlp("rm", "rm", "-rf", district, (char *)NULL);
        perror("execlp");
        exit(1);
    }
 
    int status;
    waitpid(pid, &status, 0);
 
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "rm -rf failed '%s'.\n", district);
        return -1;
    }
 
    printf("District '%s' was removed\n", district);
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
    const char *role = NULL;
    const char *user = NULL;
    const char *command = NULL;
    const char *district = NULL;
    int extra = 0;

    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "--role")             && i+1 < argc) { role     = argv[++i]; }
        else if (!strcmp(argv[i], "--user")             && i+1 < argc) { user     = argv[++i]; }
        else if (!strcmp(argv[i], "--add")              && i+1 < argc) { command  = "add";       district = argv[++i]; }
        else if (!strcmp(argv[i], "--list")             && i+1 < argc) { command  = "list";      district = argv[++i]; }
        else if (!strcmp(argv[i], "--view")             && i+2 < argc) { command  = "view";      district = argv[++i]; extra = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--remove_report")    && i+2 < argc) { command  = "remove";    district = argv[++i]; extra = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--update_threshold") && i+2 < argc) { command  = "threshold"; district = argv[++i]; extra = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--filter")           && i+1 < argc) { command  = "filter";    district = argv[++i]; }
        else if (!strcmp(argv[i], "--remove_district")  && i+1 < argc) { command  = "remove_district"; district = argv[++i];}
    }

    if (!role || !command || !district) { usage(argv[0]); return 1; }
    if (strcmp(role, "manager") != 0 && strcmp(role, "inspector") != 0) {
        fprintf(stderr, "Error: invalid role: '%s'\n", role);
        return 1;
    }
    if (!user) user = "unknown";
    if (strcmp(command, "remove_district") != 0) {
        if (init_district(district) != 0) return 1;
    }

    if      (!strcmp(command, "add"))       return add_report(district, role, user);
    else if (!strcmp(command, "list"))      return list(district, role);
    else if (!strcmp(command, "view"))      return view(district, role, extra);
    else if (!strcmp(command, "remove"))    return sterge_raport(district, role, extra);
    else if (!strcmp(command, "threshold")) return update_threshold(district, role, extra);
    else if (!strcmp(command, "remove_district")) return remove_district(district, role);
    else if (!strcmp(command, "filter")) {
        int cond_start = 0;
        for (int i = 1; i < argc; i++)
            if (!strcmp(argv[i], "--filter")) { cond_start = i + 2; break; }
        return filter(district, role,argc - cond_start,cond_start < argc ? &argv[cond_start] : NULL);
    }

    return 0;
}