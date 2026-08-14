#ifndef BUFFER_MANAGER_H
#define BUFFER_MANAGER_H

#include <stdbool.h>
#include <time.h>
#include <stdio.h>
typedef struct Block Block;

#define BUFFER_POOL_SIZE 100

// extern BufferManager g_buffer_manager

typedef struct BufferFrame{
    Block *block;       // actual block data
    int block_id;       // block id in the file
    bool is_dirty;      // dirty flag
    int pin_count;      // how many clients are using it
    time_t last_used;   // for LRU replacement
} BufferFrame;

typedef struct BufferManager {
    BufferFrame *frames;   // array of frames
    int num_frames;        // size of buffer pool
    int block_size;        // block size (bytes)
    FILE *file;            // file pointer for the underlying storage
} BufferManager;

// Initialize buffer manager with given pool size and block size
int buffer_manager_init(BufferManager *bm, const char *filename, int num_frames, int block_size);

// Shutdown: flush dirty blocks and free memory
int buffer_manager_shutdown(BufferManager *bm);

// Fetch a block into buffer pool (pins it, increases pin_count)
Block *buffer_manager_get_block(BufferManager *bm, int block_id);

// Mark a block as dirty
void buffer_manager_mark_dirty(BufferManager *bm, Block *block);

// Unpin a block (decrease pin_count, can be evicted later)
void buffer_manager_unpin(BufferManager *bm, Block *block);

// Force flush all dirty blocks to disk
int buffer_manager_flush(BufferManager *bm);

#endif
