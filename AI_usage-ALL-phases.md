# AI Usage Documentation — City Infrastructure
**Tool folosit:** Claude.ai  

---

## Phase 1 — Functiile de filtrare

### Contextul cererii

Inital programul avea: structura `Report`, functia `init_district` cu setarea permisiunilor, comenzile `add`, `list`, `view`, `remove_report` si `update_threshold`. Codul functiona dar comanda si necesita adaugarea functiei de filtrare utilizand instrumente AI.

### Promptul dat catre AI

> Am un proiect C pentru un sistem de raportare a infrastructurii urbane care lucreaza cu fisiere binare pe Linux. Structura mea de date este:
>
> ```c
> typedef struct {
>     int id;
>     char inspector[50];
>     double latitude;
>     double longitude;
>     char category[20];
>     int severity;
>     time_t timestamp;
>     char description[100];
> } Report;
> ```
>
> Comanda `filter` primeste conditii de forma `"severity:>=:2"` sau `"category:==:road"` ca argumente in linia de comanda. Am nevoie de doua functii:
>
> 1. `int parse_condition(const char *input, char *field, char *op, char *value)` care sa imparta stringul `"field:operator:value"` in cele trei componente.
>
> 2. `int match_condition(Report *r, const char *field, const char *op, const char *value)` care sa returneze 1 daca raportul satisface conditia si 0 altfel.
>
> Campurile suportate: `severity` (int), `category` (char[]), `inspector` (char[]), `timestamp` (time_t). Operatorii: `==`, `!=`, `<`, `<=`, `>`, `>=`.

### Ce a generat AI-ul

```c
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
```
```c
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
```

### Modificari fata de varianta generata

Functiile nu au fost modificate deoarece functionau insa pe parcurs am adaugat:
- printf temporare pentru a verifica ca despartirea se produce corect
- variabile pentru testare ca sa ma asigur ca valorile obtinute sunt corecte

### Ce am invatat

- cum sa transofrm valorile legate de timestemp in valori care pot fi citite de un utilizator obisnuit
- cum trebuie facute imputurile de la tastatura pentru a asigura ca sunt interpretate corespunzator

---

## Phase 2 — Procese, semnale si notificari

### Ce am folosit AI pentru in Phase 2

In Phase 2 codul de baza era functional insa am folosit uneltele AI pentru a ma asigura ca nu exista probleme pe care nu le-am observat la implementarea initala iar acesta a reusit sa identifice urmatoarele:
- verificarea eronata la stergerea unui district inexistent
- o problema legata de modul in care functia notify monitor trimitea semnalele catre monitor
- problema recurenta cand venea vorba de utilizarea printf pentru erori

### Modificari sugerate de AI in city_manager.c

**Functia `notify_monitor`:** AI-ul a sugerat folosirea `kill(pid, 0)` pentru a testa daca un proces exista fara a-i trimite un semnal real.

**Functia `remove_district`:** AI-ul a sugerat adaugarea `stat()` + `S_ISDIR()` inainte de fork pentru a confirma ca argumentul este un director existent, si inlocuirea `wait(NULL)` cu `waitpid(pid, &status, 0)` + `WIFEXITED` + `WEXITSTATUS` pentru a detecta daca `rm -rf` a esuat.

**Logging-ul:** AI-ul a sugerat folosirea `fprintf(stderr, ...)` in loc de `printf` pentru mesajele de eroare.

### Modificari sugerate de AI in monitor_reports.c

**Handlerii de semnal:** Codul inital nu se inchidea curata la primirea semnalului CTRL+C existand posibilitatea ca fisierul de monitor sa nu se stearga

**`write()` in loc de `printf()` in handler:** A sugerat folosirea `write(STDOUT_FILENO, msg, len)` direct, care este garantat sigura.

**`pause()` in bucla:** AI-ul a confirmat ca `pause()` este mai eficient decat `sleep(1)` in bucla deoarece suspenda procesul complet fara consum de CPU pana la orice semnal.

### Ce am invatat in Phase 2

- De ce  este mai bine sa folosim `write()` si nu `printf()` in handler-ii de semnal 
- Cum `kill(pid, 0)` testeaza existenta unui proces fara efecte secundare
- Diferenta dintre `wait(NULL)` si `waitpid` cu verificarea exit code-ului

---

## Phase 3 — Pipe-uri, redirectari si city_hub

### Ce am folosit AI pentru in Phase 3

Am intampinat probleme la construirea lantului de Bunic->parinte->copil pentru procese (hub → hub_mon → monitor)

### Problema principala: .monitor_pid ramanea pe disc

Codul initial trimitea `SIGTERM` la `hub_mon` cand primea `exit`, dar `hub_mon` murea imediat fara sa propaghe semnalul la monitor. Monitorul ramanea viu ca proces orfan cu `.monitor_pid` pe disc.

AI-ul a sugerat doua abordari si a recomandat-o pe cea mai simpla: in loc de a propaga semnalul prin hub_mon, comanda `exit` din hub citeste direct `.monitor_pid` si trimite `SIGINT` la monitor, lasandu-l sa se inchida singur si sa stearga fisierul.

### Optimizari sugerate pentru city_hub

**`fdopen()` + `fgets()` in loc de `read()`:** AI-ul a sugerat asocierea unui `FILE*` la capatul de citire al pipe-ului cu `fdopen()` si citirea cu `fgets()` linie cu linie, in loc de `read()` cu buffer manual.

**Verificarea existentei districtului in `calculate_scores`:** AI-ul a sugerat adaugarea `stat(district, &st)` inainte de `pipe + fork + exec scorer` pentru a sari districtele inexistente cu un mesaj clar in loc sa lase scorer-ul sa esueze cu eroare generica.

**dup2() pentru redirectarea stdout spre pipe:**  AI-ul a explicat mecanismul: dup2(pfd[1], STDOUT_FILENO) închide descriptorul 1 (stdout) și îl face să refere același canal ca pfd[1] (capătul de scriere al pipe-ului). Orice printf sau write(1, ...) din procesul fiu ajunge astfel automat în pipe.

### Ce am invatat in Phase 3

- De ce este obligatoriu sa inchizi capatul neutilizat al unui pipe inainte de `exec` — altfel `read()` nu primeste niciodata EOF si bucla de citire se blocheaza
- Diferenta practica dintre `read()` si `fdopen() + fgets()` pentru citire linie cu linie din pipe
- Importanta `fflush(stdout)` cand stdout este redirectat prin `dup2` spre un pipe

---