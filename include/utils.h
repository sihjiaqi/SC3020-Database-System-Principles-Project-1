#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <time.h>
#endif

#include "storage.h"


extern long g_block_size;
long getFileSize(const char *filename);
void print_storage_error(int error_code, const char* operation);
long get_block_size(void);
void startTimer(void);
double getElapsedTimeMillis(void);
void encodeDate(unsigned char out[3], int year, int month, int day);
void decodeDate(const unsigned char in[3], int *year, int *month, int *day);
void sleep_microseconds(long microseconds);

#endif