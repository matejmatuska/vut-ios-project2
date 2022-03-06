/*
* out_writer.h
* IOS - Projekt 2
* Matej Matuška (xmatus36)
* VUT FIT Brno
* 02.05.2021
*/
#ifndef _OUT_WRITER_H_
#define _OUT_WRITER_H_

#include <stdio.h>
#include <stdbool.h>
#include <semaphore.h>

bool out_writer_init();
void out_write(char *fmt, ...);

#endif //_OUT_WRITER_H_
