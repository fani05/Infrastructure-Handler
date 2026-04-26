# AI Usage Documentation - city_manager Phase 1
## Informatii generale

Student: Nica Fabian
Tool folosit: Claude.ai
Scop: implementarea functiilor de ajutor pentru comanda --filter

## Cererea initiala

Am nevoie de 2 functii in C pentru un sistem de rapoarte. Prima, parse_condition - are ca scop sa imparta un text precum "severity:>=:2" in 3 bucati separate. 
A doua, match_condition - primeste un raport si trebuie sa verifice daca acele conditii sunt adevarate sau nu

## Ce a generat AI-ul

AI-ul mi-a generat codul pentru cele doua functii. Pentru parse_condition a folosit functia strchr ca sa gaseasca simbolul : si sa "taie" string-ul acolo. Pentru match_condition, a facut o lista lunga de if si else if care compara fiecare camp in parte.

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

## Modificari

Functile propriuzise nu au fost modificate insa am adaugat pe parcurs:
1.printf pentru a putea verifica ca functia se integreaza cu restul programului
2.O variabila de verificare pentru a testa cum se modifica aceasta 

## Ce am invatat

Utilitatea avansata pentru functia strchr pe care AI-ul a folosit-o
Am inteles logica de transformare si separate a primului sir de caractere
Am inteles mai bine cum sa pun mesaje de verficare pe parcurs pentru a putea urmarii logica AI-ului


