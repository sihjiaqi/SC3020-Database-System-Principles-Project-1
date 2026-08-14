#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>
#include "nba_record.h"

#pragma pack(push, 4)

typedef struct Block{
    int block_id;       // block identifier 
    int record_count;   // number of records in this block
    bool is_dirty;      // if the block is not dirty skips writing back to the disk
    uint32_t timestamp; // last modified timestamp of the block
    NBA_Record records[]; // array of records
} Block;

#pragma pack(pop)

// this is the array of blocks that are loaded into memory
typedef struct BlockArray{
    Block **blocks;
    int num_blocks;
    int block_size;
} BlockArray;

typedef struct BufferManager BufferManager;

// Utility functions
void deleteRecordsLinear(BufferManager *bm, int num_blocks);

#endif // BLOCK_H
