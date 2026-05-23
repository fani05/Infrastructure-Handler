# City Infrastructure Reporting System

 Un program C care gestioneaza rapoarte de infrastructura urbana (gropi, iluminat, inundatii) pe districte, cu permisiuni Unix simulate.

## Compilare

```bash
gcc -Wall -Wextra -g -o city_manager city_manager.c
gcc -Wall -Wextra -g -o monitor_reports monitor_reports.c
gcc -Wall -Wextra -g -o scorer scorer.c
gcc -Wall -Wextra -g -o city_hub city_hub.c
```

## Comenzi districts (city_manager)

Pentru ambele roluri:
```bash
./city_manager --role manager --user Andrei --add downtown
./city_manager --role inspector --user Maria --list Centru
./city_manager --role inspector --user Maria --view Centru 2
./city_manager --role inspector --user Maria --filter Centru "severity:>=:2"
```
> Conditiile de filter trebuie puse intre ghilimele. Campuri: `severity`, `category`, `inspector`, `timestamp`. Operatori: `==`, `!=`, `<`, `<=`, `>`, `>=`.
> Pentru filtrul de timestamp data trebuie pusa in acest format: "timestamp:<:$(date -d '2026-05-24' +%s)"

Doar pentru manageri:
```bash
./city_manager --role manager --user Andrei --remove_report Centru 2
./city_manager --role manager --user Andrei --update_threshold Centru 3
./city_manager --role manager --user Andrei --remove_district Centru
```

## Monitor

```bash
./monitor_reports            # pornire
kill -2 $(cat .monitor_pid)  # oprire
```

## City Hub

```bash
./city_hub
hub> start_monitor
hub> calculate_scores downtown Centru
hub> help
hub> exit
```