#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>

#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>

#include "out_writer.h"

typedef struct {
    int cnt;
    sem_t cnt_mutex;

    int rd_cnt; // reindeer counter
    sem_t rd_mutex; // reindeer counter mutex
    sem_t hitch_sem;

    int waiting; //number of elves waiting to enter Santa Workshop
    int inside;
    bool closed; // sign put up
    sem_t elf_mutex; // counter mutex
    sem_t queue;

    sem_t santa_sleep;
    sem_t santa_wait_hitch;

    int elves;
    sem_t wait_workshop;
    sem_t wait_help;
    sem_t got_help;
} shared_mem;

// you need to call srand before calling this the first time
// return random number between from and to
int rand_range(int from, int to)
{
    int x = (rand() % (to - from + 1)) + from;
    return x;
}

void santa_process(shared_mem *shm, int ne, int nr)
{
    out_write("Santa: going to sleep\n");
    // santa woke up
    while (1)
    {
        sem_wait(&shm->santa_sleep);

        sem_wait(&shm->rd_mutex);
        if (shm->rd_cnt == 0)
        {
            // last reindeer is back
            shm->rd_cnt = nr;
            sem_post(&shm->rd_mutex);
            break;
        }
        else
        {
            out_write("Santa: helping elves\n");
            sem_post(&shm->wait_help);
            sem_post(&shm->wait_help);
            sem_post(&shm->wait_help);
            // wait until all elves leave the workshop
            sem_wait(&shm->got_help);
            out_write("Santa: going to sleep\n");
        }
        sem_post(&shm->rd_mutex);
    }

    // go hitch reindeer
    out_write("Santa: closing workshop\n");

    sem_wait(&shm->elf_mutex);
    shm->closed = true;
    sem_post(&shm->elf_mutex);

    // now hitch
    for (int i = 0; i < nr; i++)
    {
        sem_post(&shm->hitch_sem);
    }

    sem_wait(&shm->santa_wait_hitch);
    out_write("Santa: Christmas started\n");

    for (int i = 0; i < ne; i++)
    {
        sem_post(&shm->wait_help);
    }

    exit(EXIT_SUCCESS);
}

void elf_process(int elfId, int te, shared_mem *shm) {
    out_write("Elf %d: started\n", elfId);

    while (1)
    {
        // individual work
        usleep(rand_range(0, te) * 1000);

        out_write("Elf %d: need help\n", elfId);

        sem_wait(&shm->elf_mutex);
        if (shm->closed)
        {
            sem_post(&shm->elf_mutex);
            out_write("Elf %d: taking holidays\n", elfId);
            exit(EXIT_SUCCESS);
        }
        sem_post(&shm->elf_mutex);

        // need santas help!

        sem_wait(&shm->wait_workshop);
        sem_wait(&shm->elf_mutex);

        shm->waiting++;
        if (shm->waiting == 3)
        {
            // wakeup Santa!
            sem_post(&shm->santa_sleep);
        }
        else
        {
            sem_post(&shm->wait_workshop);
        }
        sem_post(&shm->elf_mutex);

        // wait for help
        sem_wait(&shm->wait_help);

        sem_wait(&shm->elf_mutex);
        if (shm->closed)
        {
            sem_post(&shm->elf_mutex);
            sem_post(&shm->wait_workshop);
            out_write("Elf %d: taking holidays\n", elfId);
            exit(EXIT_SUCCESS);
        }
        sem_post(&shm->elf_mutex);

        // get help from Santa
        out_write("Elf %d: get help\n", elfId);

        sem_wait(&shm->elf_mutex);
        shm->waiting--;
        //TODO out_write("Waiting = %d\n", shm->waiting);
        if (shm->waiting == 0)
        {
            //TODO out_write("%d All elves left, kick the santa!\n", elfId);
            sem_post(&shm->got_help);
            sem_post(&shm->wait_workshop);
        }
        sem_post(&shm->elf_mutex);
    }
}

void reindeer_process(int rdId, int tr, shared_mem *shm)
{
    out_write("RD %d: rstarted\n", rdId);

    // on vacation
    usleep(rand_range(tr / 2, tr) * 1000);

    out_write("RD %d: return home\n", rdId);

    // back from vacation
    sem_wait(&shm->rd_mutex);
    shm->rd_cnt--;
    if (shm->rd_cnt == 0)
    {
        // last reindeer is back, wakeup santa!
        sem_post(&shm->santa_sleep);
    }
    sem_post(&shm->rd_mutex);

    // wait to get hitched
    sem_wait(&shm->hitch_sem);

    // got hitched
    out_write("RD %d: get hitched\n", rdId);

    sem_wait(&shm->rd_mutex);
    shm->rd_cnt--;
    if (shm->rd_cnt == 0)
    {
        // last reindeer got hitched signal santa
        sem_post(&shm->santa_wait_hitch);
    }
    sem_post(&shm->rd_mutex);
    exit(EXIT_SUCCESS);
}

shared_mem *init_shared_mem(int nr)
{
    // fd = -1 for better compatibility
    shared_mem *shm = mmap(NULL, sizeof(shared_mem),
            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

   if (shm == MAP_FAILED)
       return NULL;

    //TODO sem_init error handling

    sem_init(&shm->cnt_mutex, 1, 1);

    sem_init(&shm->rd_mutex, 1, 1);

    sem_init(&shm->hitch_sem, 1, 0);

    sem_init(&shm->elf_mutex, 1, 1);

    sem_init(&shm->queue, 1, 0);

    sem_init(&shm->santa_sleep, 1, 0);

    sem_init(&shm->santa_wait_hitch, 1, 0);

    sem_init(&shm->wait_workshop, 1, 1);

    sem_init(&shm->wait_help, 1, 0);

    sem_init(&shm->got_help, 1, 0);

    shm->rd_cnt = nr;
    shm->cnt = 0;
    shm->inside = 0;
    shm->waiting = 0;
    shm->closed = false;
    return shm;
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        fprintf(stderr, "Usage: proj2 NE NR TE TR\n");
        return EXIT_FAILURE;
    }

    char *endptr;
    int ne = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0')
    {
        fprintf(stderr, "Error: NE must be an intenger\n");
        return EXIT_FAILURE;
    }
    else if (ne <= 0 || ne >= 1000)
    {
        fprintf(stderr, "Error: NE must be 0 < NE < 1000\n");
        return EXIT_FAILURE;
    }

    int nr = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0')
    {
        fprintf(stderr, "Error: NR must be an intenger\n");
        return EXIT_FAILURE;
    }
    else if (nr <= 0 || nr >= 20)
    {
        fprintf(stderr, "Error: NR must be 0 < NR < 20\n");
        return EXIT_FAILURE;
    }

    int te = strtol(argv[3], &endptr, 10);
    if (*endptr != '\0')
    {
        fprintf(stderr, "Error: TE must be an intenger\n");
        return EXIT_FAILURE;
    }
    else if (te < 0 || te > 1000)
    {
        fprintf(stderr, "Error: TE must be 0 <= TE <= 1000\n");
        return EXIT_FAILURE;
    }

    int tr = strtol(argv[4], &endptr, 10);
    if (*endptr != '\0')
    {
        fprintf(stderr, "Error: TR must be an intenger\n");
        return EXIT_FAILURE;
    }
    else if (tr < 0 || tr > 1000)
    {
        fprintf(stderr, "Error: TR must be 0 <= TR <= 1000\n");
        return EXIT_FAILURE;
    }

    FILE *f = fopen("proj2.out", "w");
    out_writer_init(f);

    shared_mem *shm = init_shared_mem(nr);
    if (shm == NULL)
    {
        //TODO error handling
        fprintf(stderr, "Error: Failed to init shared memory");
        return EXIT_FAILURE;
    }

    // fork santa
    pid_t santa = fork();
    if (santa == 0)
    {
        srand(time(NULL) * getpid());
        santa_process(shm, ne, nr);
        return EXIT_FAILURE;
    }
    else if (santa == -1)
    {
        return EXIT_FAILURE;
    }

    // fork elves
    for (int i = 1; i <= ne; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            srand(time(NULL) * getpid());
            elf_process(i, te, shm);
        }
        else if (pid == -1)
        {
            fprintf(stderr, "Error: Failed forking elf!\n");
            return EXIT_FAILURE;
        }
    }

    // fork reindeers
    for (int i = 1; i <= nr; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            srand(time(NULL) * getpid());
            reindeer_process(i, tr, shm);
        }
        else if (pid == -1)
        {
            fprintf(stderr, "Error: Failed forking reindeer!\n");
            return EXIT_FAILURE;
        }
    }

    while(wait(NULL) > 0); // wait for all child processes

    fclose(f);

    return EXIT_SUCCESS;
}
