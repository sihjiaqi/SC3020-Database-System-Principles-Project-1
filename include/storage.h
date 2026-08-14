#ifndef STORAGE_H
#define STORAGE_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "block.h"

// Error codes for the storage component
#define STORAGE_SUCCESS 0 
#define STORAGE_ERROR_FILE_NOT_FOUND -1
#define STORAGE_ERROR_MEMORY_ALLOCATION -2
#define STORAGE_ERROR_INVALID_PARAMETER -3
#define STORAGE_ERROR_IO_ERROR -4
#define STORAGE_ERROR_CHECKSUM_MISMATCH -5

// read data from the text file and load into the records array
int read_data_from_file(const char*filename, NBA_Record **records, int *num_records);

#endif
