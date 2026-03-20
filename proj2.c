// 2 Project IOS
// Shelest Oleksii
// xshele02

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SHM_LINE "/xshele02_line" // Definice nazvu sdilene pameti

int L;  // pocet lyzaru     -> L < 20000
int Z;  // pocet zastavek   -> 0 < Z <= 10
int K;  // kapacita skibusu -> 10 <= K <= 100
int TL; // Maximalni cas v mikrosekundach, ktery lyzar ceka, nez prijde na zastavku  -> 0 <= TL <= 10000
int TB; // Maximalni doba jizdy autobusu mezi dvema zastavkami                       -> 0 <= TB <= 1000

int *num_line;                // Promenna pro ulozeni cisla radku souboru
int shm_new_line;             // Sdilena pamet, ktera bude pouzita k ulozeni promenne num_line
int **skiers_at_stop;         // Pole pro sledovani lyzaru na autobusovych zastavkach
int *skiers_in_bus_arr;       // Pole pro sledovani lyzaru v autobusu
int skiers_in_bus = 0;        // Pocet lyzaru v skibuse
int skiers_all_delivered = 0; // Pocet lyzaru, ktere uz 'going ski'

FILE *output_file; // Promenna pro soubor

// Promenne pro procesy
pid_t skibus_process;
pid_t lyzar_process;

sem_t *semVypis;     // Semafor pro zapis dat do souboru
sem_t **semZastavka; // Semafory pro zastavky

// Definice jednotlivych funkci
void start();
void clean();
void skier_function(int skier_id);
void skibus_function();
bool has_waiting_skiers(int stop_id);
void board_skiers(int stop_id);

// Funkce pro overeni, je element int
bool is_integer(const char *str)
{
    char *endptr;
    strtol(str, &endptr, 10);
    return (*endptr == '\0');
}

int main(int argc, char *argv[])
{
    // Overeni, kolik je argumentu
    if (argc < 6)
    {
        fprintf(stderr, "Error: Incorrect number of arguments\n");
        exit(1);
    }
    // Kontrola, ze vsichni argumenty jsou int
    for (int i = 1; i < argc; i++)
    {
        if (!is_integer(argv[i]))
        {
            fprintf(stderr, "Error: Incorrect type of arguments\n");
            exit(1);
        }
    }
    // Inicializace parametru
    L = atoi(argv[1]);
    Z = atoi(argv[2]);
    K = atoi(argv[3]);
    TL = atoi(argv[4]);
    TB = atoi(argv[5]);

    // Overeni hodnot argumentu
    if (!(L < 20000))
    {
        fprintf(stderr, "Error: Incorrect value of arguments -> L\n");
        exit(1);
    }
    if (!(0 < Z && Z <= 10))
    {
        fprintf(stderr, "Error: Incorrect value of arguments -> Z\n");
        exit(1);
    }
    if (!(10 <= K && K <= 100))
    {
        fprintf(stderr, "Error: Incorrect value of arguments -> K\n");
        exit(1);
    }
    if (!(0 <= TL && TL <= 10000))
    {
        fprintf(stderr, "Error: Incorrect value of arguments -> TL\n");
        exit(1);
    }
    if (!(0 <= TB && TB <= 1000))
    {
        fprintf(stderr, "Error: Incorrect value of arguments -> TB\n");
        exit(1);
    }

    output_file = fopen("proj2.out", "w"); // Otevirame soubor pro zapis
    if (output_file == NULL)               // kdyz chyba
    {
        fprintf(stderr, "Error: Could not open up an output file\n");
        exit(1);
    }
    start(); // volame funkce pro inicializace sdilene pameti a semaforu

    skibus_process = fork(); // zaciname process skibus
    if (skibus_process < 0)  // kdyz fork() vraci < 0 -> chyba
    {
        fprintf(stderr, "Error: Could not fork bus_process\n");
        clean();             // volame funkce pro smazani sdilene pameti a semaforu
        fclose(output_file); // zavirame soubor
        exit(1);
    }
    else if (skibus_process == 0) // kdyz process -> potomek
    {
        skibus_function(); // volame funkce pro skibus
        exit(0);
    }

    // Tady delame L processu pro lyzaru
    for (int i = 1; i <= L; i++)
    {
        lyzar_process = fork(); // zaciname process lyzar
        if (lyzar_process < 0)  // kdyz fork() vraci < 0 -> chyba
        {
            fprintf(stderr, "Error: Could not fork lyzar_process\n");
            clean();
            fclose(output_file);
            exit(1);
        }
        else if (lyzar_process == 0) // kdyz process -> potomek
        {
            sem_wait(semVypis);                                            // zavirame semafor pro vypis
            fprintf(output_file, "%d: L %d: started\n", ++(*num_line), i); // zapiseme do souboru
            fflush(output_file);                                           // bez bufferu
            sem_post(semVypis);                                            // otevirame soubor pro vypis
            skier_function(i);                                             // volame funkce pro lyzaru s argumentem i ->(L id)
            exit(0);
        }
    }
    int children_count = L + 1; // L * lyzar_process + skibus_process
    for (int i = 0; i < children_count; i++)
    {
        wait(NULL); // cekame na ukonceni vsech processu
    }

    clean();             // volame funkce pro smazani sdilene pameti a semaforu
    fclose(output_file); // zavirame soubor
    exit(0);             // vratime 0
}

// Funkce pro inicializace sdilene pameti a semaforu
void start()
{
    // Vytvoreni segmentu sdilene pameti pro semafor
    semVypis = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);

    // Kontrola uspesnosti
    if (semVypis == MAP_FAILED)
    {
        fprintf(stderr, "Error creating semaphores\n");
        exit(1);
    }
    // Inicializace semaforu
    if (sem_init(semVypis, 1, 1) == -1)
    {
        fprintf(stderr, "Error initializing semaphores\n");
        exit(1);
    }
    // Odstraneni predchoziho segmentu sdilene pameti, pokud existuje
    shm_unlink(SHM_LINE);

    // Vytvoreni noveho segmentu sdilene pameti pro promennou num_line
    shm_new_line = shm_open(SHM_LINE, O_CREAT | O_RDWR, S_IWUSR | S_IRUSR);

    // Kontrola uspesnosti
    if (shm_new_line == -1)
    {
        fprintf(stderr, "Error: Problem with memory\n");
        exit(1);
    }

    // Nastaveni velikosti segmentu sdilene pameti
    if (ftruncate(shm_new_line, sizeof(int)) == -1)
    {
        fprintf(stderr, "Error: Problem truncating memory\n");
        exit(1);
    }

    // Pripojeni segmentu sdilene pameti k promenne num_line
    num_line = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_new_line, 0);

    // Kontrola uspesnosti
    if (num_line == MAP_FAILED)
    {
        fprintf(stderr, "Error mapping memory\n");
        exit(1);
    }

    // Alokace pameti pro pole semaforu pro kazdou zastavku
    semZastavka = malloc(Z * sizeof(sem_t *));

    // Kontrola uspesnosti
    if (semZastavka == NULL)
    {
        fprintf(stderr, "Error allocating memory for semaphores\n");
        exit(1);
    }

    // Inicializace semaforu pro kazdou zastavku
    for (int i = 0; i < Z; i++)
    {
        semZastavka[i] = malloc(sizeof(sem_t));
        if (semZastavka[i] == NULL)
        {
            fprintf(stderr, "Error allocating memory for semaphore\n");
            exit(1);
        }
        char sem_name[50];
        sprintf(sem_name, "/semZastavka_%d", i + 1);
        semZastavka[i] = sem_open(sem_name, O_CREAT, S_IWUSR | S_IRUSR, 1);
        if (semZastavka[i] == SEM_FAILED)
        {
            fprintf(stderr, "Error initializing semaphore\n");
            exit(1);
        }
    }
    // Alokace pameti pro pole ukazatelu pro kazdou zastavku
    skiers_at_stop = mmap(NULL, Z * sizeof(int *), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);

    // Alokace pameti pro pole ukazatelu pro uchovani informaci o lyzarich v skibuse
    skiers_in_bus_arr = mmap(NULL, K * sizeof(int *), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);

    // Kontrola uspesnosti
    if (skiers_at_stop == MAP_FAILED || skiers_in_bus_arr == MAP_FAILED)
    {
        fprintf(stderr, "Error mapping memory\n");
        exit(1);
    }

    // Inicializace pole ukazatelu pro kazdou zastavku
    for (int i = 0; i < Z; i++)
    {
        skiers_at_stop[i] = mmap(NULL, (L + 1) * sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
        if (skiers_at_stop[i] == MAP_FAILED)
        {
            fprintf(stderr, "Error mapping memory\n");
            exit(1);
        }
        skiers_at_stop[i][0] = 0; // na zacatku 0 lyzaru
    }
    // Inicializace pole ukazatelu pro uchování informaci o lyzarich v skibuse
    for (int i = 1; i <= K; i++)
    {
        skiers_in_bus_arr[i] = -1; // na zacatku zadneho nemame
    }
}

// Funkce pro smazani sdilene pameti a semaforu
void clean()
{
    sem_destroy(semVypis);
    munmap(semVypis, sizeof(sem_t));

    shm_unlink(SHM_LINE);
    for (int i = 0; i < Z; i++)
    {
        sem_close(semZastavka[i]);
        char sem_name[50];
        sprintf(sem_name, "/semZastavka_%d", i + 1);
        sem_unlink(sem_name);
    }
    free(semZastavka);

    for (int i = 0; i < Z; i++)
    {
        munmap(skiers_at_stop[i], (L + 1) * sizeof(int));
    }
    munmap(skiers_at_stop, Z * sizeof(int *));
    munmap(skiers_in_bus_arr, K * sizeof(int *));
}

// Funkce pro zpracovani lyzaru
void skier_function(int skier_id)
{
    srand(time(NULL) * getpid()); // rand() se aktualizuje pokaždé
    usleep(rand() % (TL + 1));    // cekame nejaky cas, nez prijde na zastavku
    int stop_id = rand() % Z + 1; // cislo zastavky

    sem_wait(semVypis); // zavirame semafor pro vypis
    fprintf(output_file, "%d: L %d: arrived to %d\n", ++(*num_line), skier_id, stop_id);
    fflush(output_file);
    sem_post(semVypis); // otevirame semafor

    sem_wait(semZastavka[stop_id - 1]);                                     // zavirame semafor pro danou zastavku
    skiers_at_stop[stop_id - 1][0]++;                                       // zvetsime mnozstvi lyzaru na zastavce
    skiers_at_stop[stop_id - 1][skiers_at_stop[stop_id - 1][0]] = skier_id; // zapiseme id lyzaru do pole
    sem_post(semZastavka[stop_id - 1]);                                     // otevirame semafor
}

// Funkce pro zpracovani skibusu
void skibus_function()
{
    srand(time(NULL) * getpid()); // rand() se aktualizuje pokaždé
    sem_wait(semVypis);           // zavirame semafor pro vypis
    fprintf(output_file, "%d: BUS: started\n", ++(*num_line));
    fflush(output_file);
    sem_post(semVypis); // otevirame semafor

    int stop_id = 1; // na zacatku cislo zastavky -> 1

    while (1) // dokud všichni nedorazí.
    {
        usleep(rand() % (TB + 1)); // cekame nejaky cas, nez prijede na zastavku

        sem_wait(semVypis); // zavirame semafor pro vypis
        fprintf(output_file, "%d: BUS: arrived to %d\n", ++(*num_line), stop_id);
        fflush(output_file);
        sem_post(semVypis); // otevirame semafor

        if (has_waiting_skiers(stop_id)) // kdyz nekdo ceka na zastavce
        {
            board_skiers(stop_id); // volame funkce pro nastupovani lyzaru do skibusu
        }

        sem_wait(semVypis); // zavirame semafor pro vypis
        fprintf(output_file, "%d: BUS: leaving %d\n", ++(*num_line), stop_id);
        fflush(output_file);
        sem_post(semVypis); // otevirame semafor

        if (stop_id == Z) // kdyz zastavka je konecna
        {

            sem_wait(semVypis); // zavirame semafor pro vypis
            fprintf(output_file, "%d: BUS: arrived to final\n", ++(*num_line));
            fflush(output_file);
            sem_post(semVypis); // otevirame semafor

            sem_wait(semVypis);                      // zavirame semafor pro vypis
            for (int i = 1; i <= skiers_in_bus; i++) // vsichni, kdo v skibusu, 'going to ski'
            {
                fprintf(output_file, "%d: L %d: going to ski\n", ++(*num_line), skiers_in_bus_arr[i]);
            }
            fflush(output_file);
            sem_post(semVypis); // otevirame semafor

            sem_wait(semVypis); // zavirame semafor pro vypis
            fprintf(output_file, "%d: BUS: leaving final\n", ++(*num_line));
            fflush(output_file);
            sem_post(semVypis); // otevirame semafor

            skiers_all_delivered += skiers_in_bus;

            int passengers_waiting = 0; // bool promenna pro overeni

            // Kdyz na nejake zastavce nekdo ceka -> zaciname znovu od 1 zastavky
            // Kdyz ne a uz vsechne lyzary 'going to ski' -> konec
            for (int i = 1; i <= Z; ++i)
            {
                if (has_waiting_skiers(i))
                {
                    passengers_waiting = 1;
                    break;
                }
            }
            if (!passengers_waiting && skiers_all_delivered == L)
            {
                break;
            }
            else
            {
                stop_id = 1;
                skiers_in_bus = 0;
            }
        }
        else
        {
            stop_id++; // zvetsime cislo zastavky
        }
    }

    sem_wait(semVypis); // zavirame semafor pro vypis
    fprintf(output_file, "%d: BUS: finish\n", ++(*num_line));
    fflush(output_file);
    sem_post(semVypis); // otevirame semafor
}

// Funcke pro kontrolu lyzaru na zastavkach
bool has_waiting_skiers(int stop_id)
{
    sem_wait(semZastavka[stop_id - 1]);
    bool result = (skiers_at_stop[stop_id - 1][0] > 0); // kdyz pocet lyzaru na zastavce > 0 -> true
    sem_post(semZastavka[stop_id - 1]);
    return result;
}

// Funkce pro nastupovani lyzaru do skibusu
void board_skiers(int stop_id)
{
    int remaining_seats = K - skiers_in_bus; // kolik jeste muze skibus vzit
    if (remaining_seats > 0)
    {
        // Kdyz lyzaru na zastavce je mensi, nez je volnych mist v skibusu -> pocet lyzaru na zastavce
        // Kdyz naopak -> pocet volnych mist
        int boarding_count = (remaining_seats < skiers_at_stop[stop_id - 1][0]) ? remaining_seats : skiers_at_stop[stop_id - 1][0];

        sem_wait(semVypis);                       // zavirame semafor pro vypis
        for (int i = 1; i <= boarding_count; i++) // vsichni nastupuji do skibusu
        {
            fprintf(output_file, "%d: L %d: boarding\n", ++(*num_line), skiers_at_stop[stop_id - 1][i]);
        }
        fflush(output_file);
        sem_post(semVypis); // otevirame semafor

        sem_wait(semZastavka[stop_id - 1]);       // zavirame semafor pro zastavku
        for (int i = 1; i <= boarding_count; i++) // presun lyzaru z zastavky do skibusu
        {
            skiers_in_bus_arr[skiers_in_bus + i] = skiers_at_stop[stop_id - 1][i];
        }
        sem_post(semZastavka[stop_id - 1]); // otevirame semafor

        sem_wait(semZastavka[stop_id - 1]); // zavirame semafor pro zastavku

        // Aktualizace poctu lyzaru na zastavce
        for (int i = 1; i <= skiers_at_stop[stop_id - 1][0] - boarding_count; i++)
        {
            skiers_at_stop[stop_id - 1][i] = skiers_at_stop[stop_id - 1][i + boarding_count];
        }
        skiers_at_stop[stop_id - 1][0] -= boarding_count;

        sem_post(semZastavka[stop_id - 1]); // otevirame semafor

        skiers_in_bus += boarding_count; // aktualizace poctu lyzaru v skibuse
    }
}