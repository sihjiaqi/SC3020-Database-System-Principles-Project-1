#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#include "storage.h"

long g_block_size=0;


long getFileSize(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return size;
}

void print_storage_error(int error_code, const char* operation) {
    printf("ERROR during %s: ", operation);
    
    switch (error_code) {
        case STORAGE_ERROR_FILE_NOT_FOUND:
            printf("File not found\n");
            break;
        case STORAGE_ERROR_MEMORY_ALLOCATION:
            printf("Memory allocation failed\n");
            break;
        case STORAGE_ERROR_INVALID_PARAMETER:
            printf("Invalid parameter\n");
            break;
        case STORAGE_ERROR_IO_ERROR:
            printf("I/O error\n");
            break;
        case STORAGE_ERROR_CHECKSUM_MISMATCH:
            printf("Data corruption detected (checksum mismatch)\n");
            break;
        default:
            printf("Unknown error code: %d\n", error_code);
    }
}

// Encode YYYY-MM-DD into 3 bytes
void encodeDate(unsigned char out[3], int year, int month, int day) {
    unsigned int val = ((year & 0xFFF) << 9) | ((month & 0xF) << 5) | (day & 0x1F);
    out[0] = val & 0xFF;
    out[1] = (val >> 8) & 0xFF;
    out[2] = (val >> 16) & 0xFF;
}

// Decode 3 bytes into YYYY, MM, DD
void decodeDate(const unsigned char in[3], int *year, int *month, int *day) {
    unsigned int val = in[0] | (in[1] << 8) | (in[2] << 16);
    *day   = val & 0x1F;
    *month = (val >> 5) & 0xF;
    *year  = (val >> 9) & 0xFFF;
}

long get_block_size(void) {
    long block_size = 0;

    #ifdef _WIN32
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        block_size = si.dwPageSize;
    #elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
        long res = sysconf(_SC_PAGESIZE);
        if (res == -1) {
            perror("Failed to get block size");
            exit(EXIT_FAILURE);
        }
        block_size = res;
    #else
        fprintf(stderr, "Unsupported operating system.\n");
        exit(EXIT_FAILURE);
    #endif

    return block_size;
}

#ifdef _WIN32
static LARGE_INTEGER startCount;
static LARGE_INTEGER freq;

void startTimer(void) {
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&startCount);
}

double getElapsedTimeMillis(void) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - startCount.QuadPart) * 1000.0 / freq.QuadPart;
}

#else
static struct timespec startTime;

void startTimer(void) {
    clock_gettime(CLOCK_MONOTONIC, &startTime);
}

double getElapsedTimeMillis(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double elapsed = (now.tv_sec - startTime.tv_sec) * 1000.0;
    elapsed += (now.tv_nsec - startTime.tv_nsec) / 1000000.0;
    return elapsed;
}
#endif

// ---------------- Cross-platform microsecond sleep ----------------
#ifdef _WIN32
void sleep_microseconds(long microseconds) {
    LARGE_INTEGER freq, start, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    long long targetTicks = (long long)((microseconds * (double)freq.QuadPart) / 1000000.0);

    do {
        QueryPerformanceCounter(&now);
    } while ((now.QuadPart - start.QuadPart) < targetTicks);
}

#else
void sleep_microseconds(long microseconds) {
    struct timespec ts;
    ts.tv_sec = microseconds / 1000000;
    ts.tv_nsec = (microseconds % 1000000) * 1000;
    nanosleep(&ts, NULL);
}
#endif
