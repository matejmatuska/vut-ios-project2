#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

#include <semaphore.h>
#include <sys/mman.h>

#include "out_writer.h"

static struct {
    unsigned A;
    sem_t mutex;
} *counter;

static FILE *file;

// return true on success, false otherwise
bool out_writer_init(FILE *f)
{
    file = f;

     // fd = -1 for better compatibility
    counter = mmap(NULL, sizeof(counter),
            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (counter == MAP_FAILED)
        return false;

    counter->A = 1;

    if (sem_init(&counter->mutex, 1, 1) == -1)
        return NULL;

    return true;
}

void out_write(char *fmt, ...)
{
    sem_wait(&counter->mutex);
    fprintf(file, "%d: ", counter->A++);

    va_list args;
    va_start(args, fmt);
    vfprintf(file, fmt, args);
    va_end(args);

    fflush(file);
    sem_post(&counter->mutex);
}
