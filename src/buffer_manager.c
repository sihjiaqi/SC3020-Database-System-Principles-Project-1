#include <stdlib.h>
#include <string.h>
#include "storage.h"
#include "buffer_manager.h"
#include "block.h"
#include <time.h>
#include "utils.h"

// BufferManager g_buffer_manager;  // actual instance

// Find a frame with given block_id
static int find_frame(BufferManager *bm, int block_id) {
    for (int i = 0; i < bm->num_frames; i++) {
        if (bm->frames[i].block && bm->frames[i].block_id == block_id) {
            return i;
        }
    }
    return -1;
}

// Pick a victim frame using LRU (skip pinned frames)
static int pick_victim(BufferManager *bm) {
    int victim = -1;
    time_t oldest = time(NULL);

    for (int i = 0; i < bm->num_frames; i++) {
        if (bm->frames[i].pin_count == 0) {
            if (!bm->frames[i].block) return i; // free slot
            if (bm->frames[i].last_used <= oldest) {
                oldest = bm->frames[i].last_used;
                victim = i;
            }
        }
    }
    return victim;
}

int buffer_manager_init(BufferManager *bm, const char *filename, int num_frames, int block_size) {
    bm->frames = (BufferFrame*)calloc(num_frames, sizeof(BufferFrame));
    if (!bm->frames) return STORAGE_ERROR_MEMORY_ALLOCATION;

    bm->num_frames = num_frames;
    bm->block_size = block_size;
    bm->file = fopen(filename, "rb+");
    if (!bm->file) {
        bm->file = fopen(filename, "wb+"); // create if not exists
        if (!bm->file) {
            free(bm->frames);
            return STORAGE_ERROR_FILE_NOT_FOUND;
        }
    }
    return STORAGE_SUCCESS;
}

int buffer_manager_shutdown(BufferManager *bm) {
    if (!bm) return STORAGE_ERROR_INVALID_PARAMETER;
    buffer_manager_flush(bm);
    for (int i = 0; i < bm->num_frames; i++) {
        if (bm->frames[i].block) free(bm->frames[i].block);
    }
    free(bm->frames);
    fclose(bm->file);
    return STORAGE_SUCCESS;
}

Block *buffer_manager_get_block(BufferManager *bm, int block_id) {
    // printf("Requesting block %d\n", block_id);
    int frame_idx = find_frame(bm, block_id);
    if (frame_idx >= 0) {
        bm->frames[frame_idx].pin_count++;
        bm->frames[frame_idx].last_used = time(NULL);
        return bm->frames[frame_idx].block;
    }

    // add a delay of 0.04 ms here
    sleep_microseconds(40);

    // Not found, need to load
    int victim = pick_victim(bm);
    if (victim < 0) return NULL; // all pinned

    BufferFrame *frame = &bm->frames[victim];
    if (frame->block) {
        if (frame->is_dirty) {
            // Write back to disk
            long offset = frame->block_id * bm->block_size;
            fseek(bm->file, offset, SEEK_SET);
            fwrite(frame->block, 1, bm->block_size, bm->file);
        }
        free(frame->block);
    }

    frame->block = (Block*)malloc(bm->block_size);
    if (!frame->block) return NULL;

    long offset = block_id * bm->block_size;
    fseek(bm->file, 0, SEEK_END);
    long file_size = ftell(bm->file);

    if (offset < file_size) {
        fseek(bm->file, offset, SEEK_SET);
        fread(frame->block, 1, bm->block_size, bm->file);
    } else {
        memset(frame->block, 0, bm->block_size);
        frame->block->block_id = block_id;
    }

    frame->block_id = block_id;
    frame->is_dirty = false;
    frame->pin_count = 1;
    frame->last_used = time(NULL);

    return frame->block;
}

void buffer_manager_mark_dirty(BufferManager *bm, Block *block) {
    for (int i = 0; i < bm->num_frames; i++) {
        if (bm->frames[i].block == block) {
            bm->frames[i].is_dirty = true;
            return;
        }
    }
}

void buffer_manager_unpin(BufferManager *bm, Block *block) {
    for (int i = 0; i < bm->num_frames; i++) {
        if (bm->frames[i].block == block) {
            if (bm->frames[i].pin_count > 0) bm->frames[i].pin_count--;
            return;
        }
    }
}

int buffer_manager_flush(BufferManager *bm) {
    for (int i = 0; i < bm->num_frames; i++) {
        if (bm->frames[i].block && bm->frames[i].is_dirty) {
            long offset = bm->frames[i].block_id * bm->block_size;
            fseek(bm->file, offset, SEEK_SET);
            fwrite(bm->frames[i].block, 1, bm->block_size, bm->file);
            bm->frames[i].is_dirty = false;
        }
    }
    fflush(bm->file);
    return STORAGE_SUCCESS;
}
