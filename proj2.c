#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <semaphore.h>

#include <sys/types.h>
#include <unistd.h>

typedef struct {
    int A;
    sem_t sem;
} counter_t;

// you need to call srand before calling this the first time
// return random number between from and to
int rand_range(int from, int to)
{
    return (rand() % (to - from + 1)) + from;
}

void santa_process(counter_t *cnt)
{
    sem_wait(&cnt->sem);
    printf("%d: Santa: going to sleep\n", cnt->A++);
    sem_post(&cnt->sem);

    exit(EXIT_SUCCESS);
}

void elf_process(int elfId, int te, counter_t *cnt)
{
    sem_wait(&cnt->sem);
    printf("%d: Elf %d: started\n", cnt->A++, elfId);
    sem_post(&cnt->sem);

    // individual work
    usleep(rand_range(0, te));

    // need santas help!
    sem_wait(&cnt->sem);
    printf("%d: Elf %d: need help\n", cnt->A++, elfId);
    sem_post(&cnt->sem);


    exit(EXIT_SUCCESS);
}

void reindeer_process(int rdId, int tr, counter_t *cnt)
{
    sem_wait(&cnt->sem);
    printf("%d: RD %d: rstarted\n", cnt->A++, rdId);
    sem_post(&cnt->sem);

    // on vacation
    usleep(rand_range(tr / 2, tr));

    sem_wait(&cnt->sem);
    printf("%d: RD %d: return home\n", cnt->A++, rdId);
    sem_post(&cnt->sem);


    exit(EXIT_SUCCESS);
}

counter_t *init_counter()
{
    // fd = -1 for better compatibility
    counter_t *cnt = mmap(NULL, sizeof(counter_t),
            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (cnt == MAP_FAILED)
        return NULL;

    cnt->A = 1;

    if (sem_init(&cnt->sem, 1, 1) == -1)
        return NULL;

    return cnt;
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

    srand(time(NULL));

    counter_t *cnt = init_counter();

    // fork santa
    pid_t santa = fork();
    if (santa == 0)
    {
        santa_process(cnt);
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
            elf_process(i, te, cnt);
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
            reindeer_process(i, tr, cnt);
        }
        else if (pid == -1)
        {
            fprintf(stderr, "Error: Failed forking reindeer!\n");
            return EXIT_FAILURE;
        }
    }

    while(wait(NULL) > 0);
    printf("Main process exiting!\n");

    if (munmap(cnt, sizeof(cnt)) != 0)
    {
        printf("Failed to munmap!\n");
    }

    return EXIT_SUCCESS;
}
