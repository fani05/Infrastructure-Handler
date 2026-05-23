#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

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

#define MAX_INSPECTORS 100
struct {
    char name[50];
    int  total_severity;
    int  num_reports;
} inspectors[MAX_INSPECTORS];
static int num_inspectors = 0;

static int find_or_add(const char *name) {
    for (int i = 0; i < num_inspectors; i++)
        if (strcmp(inspectors[i].name, name) == 0)
            return i;
    if (num_inspectors < MAX_INSPECTORS) {
        strncpy(inspectors[num_inspectors].name, name, 49);
        inspectors[num_inspectors].total_severity = 0;
        inspectors[num_inspectors].num_reports = 0;
        return num_inspectors++;
    }
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Utilizare: %s <district>\n", argv[0]);
        return 1;
    }
    const char *district = argv[1];

    char path[256];
    snprintf(path, sizeof(path), "%s/reports.dat", district);

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        printf("SCORER:%s:ERROR: can't open %s\n", district, path);
        return 1;
    }

    Report r;
    while (read(fd, &r, sizeof(Report)) == (ssize_t)sizeof(Report)) {
        int idx = find_or_add(r.inspector);
        if (idx >= 0) {
            inspectors[idx].total_severity += r.severity;
            inspectors[idx].num_reports++;
        }
    }
    close(fd);

    printf("=== Workload scores for district: '%s' ===\n", district);
    if (num_inspectors == 0) {
        printf("  (niciun raport)\n");
    } else {
        for (int i = 0; i < num_inspectors; i++) {
            printf("  %-20s score=%-4d (%d rapoarte)\n",
                   inspectors[i].name,
                   inspectors[i].total_severity,
                   inspectors[i].num_reports);
        }
    }

    return 0;
}