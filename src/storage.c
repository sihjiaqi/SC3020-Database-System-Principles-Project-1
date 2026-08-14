#include "storage.h"
#include "block.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nba_record.h>
#include <buffer_manager.h>
#include "utils.h"
// Read data from text file into records array
int read_data_from_file(const char *filename, NBA_Record **records, int *num_records) {
    if (!filename || !records || !num_records) {
        return STORAGE_ERROR_INVALID_PARAMETER;
    }

    FILE *file = fopen(filename, "r");
    if (!file) {
        return STORAGE_ERROR_FILE_NOT_FOUND;
    }

    const int CHUNK_SIZE = 1000;
    int allocated_records = CHUNK_SIZE;
    *records = (NBA_Record*)malloc(allocated_records * sizeof(NBA_Record));
    if (!*records) {
        fclose(file);
        return STORAGE_ERROR_MEMORY_ALLOCATION;
    }

    // Skip header
    char line[512];
    if (!fgets(line, sizeof(line), file)) {
        free(*records);
        *records = NULL;
        fclose(file);
        return STORAGE_ERROR_IO_ERROR;
    }

    int i = 0;
    while (fgets(line, sizeof(line), file)) {
        if (i >= allocated_records) {
            allocated_records += CHUNK_SIZE;
            NBA_Record *new_records = (NBA_Record*)realloc(*records, allocated_records * sizeof(NBA_Record));
            if (!new_records) {
                free(*records);
                *records = NULL;
                fclose(file);
                return STORAGE_ERROR_MEMORY_ALLOCATION;
            }
            *records = new_records;
        }

        memset(&((*records)[i]), 0, sizeof(NBA_Record));

        char date_str[16];
        int temp_win;

        int parsed = sscanf(line, "%10s %d %d %f %f %f %hd %hd %d",
               date_str,
               &(*records)[i].team_id_home,
               &(*records)[i].pts_home,
               &(*records)[i].fg_pct_home,
               &(*records)[i].ft_pct_home,
               &(*records)[i].fg3_pct_home,
               &(*records)[i].ast_home,
               &(*records)[i].reb_home,
               &temp_win);

        if (parsed == 9) {
            // Parse dd/mm/yyyy
            int day, month, year;
            if (sscanf(date_str, "%d/%d/%d", &day, &month, &year) == 3) {
                encodeDate((*records)[i].game_date_est, year, month, day);
            }

            (*records)[i].home_team_wins = (bool)temp_win;
            (*records)[i].is_valid = true;
            i++;
        }
    }

    *num_records = i;
    fclose(file);

    return STORAGE_SUCCESS;
}

// Delete a record using buffer manager (mark dirty inside buffer pool)
void delete_record_from_block(BufferManager *bm, int block_id, int record_index) {
    Block *block = buffer_manager_get_block(bm, block_id);
    if (!block) return;
    if (record_index >= block->record_count) {
        buffer_manager_unpin(bm, block);
        return;
    }

    block->records[record_index].is_valid = false;
    buffer_manager_mark_dirty(bm, block);
    buffer_manager_unpin(bm, block);
}

// Print a block fetched from buffer manager
void print_block_from_manager(BufferManager *bm, int block_id) {
    Block *block = buffer_manager_get_block(bm, block_id);
    if (!block) return;

    printf("=== Block ID: %d ===\n", block->block_id);
    printf("Record count: %d\n", block->record_count);
    printf("Timestamp: %u\n", block->timestamp);

    for (int i = 0; i < block->record_count; i++) {
        NBA_Record *r = &block->records[i];
        if (!r->is_valid) continue;

        int year, month, day;
        decodeDate(r->game_date_est, &year, &month, &day);

        printf("Record %d: date=%02d/%02d/%04d, team_id_home=%d, pts_home=%d, fg=%.2f, ft=%.2f, fg3=%.2f, ast=%d, reb=%d, win=%d\n",
               i,
               day, month, year,
               r->team_id_home,
               r->pts_home,
               r->fg_pct_home,
               r->ft_pct_home,
               r->fg3_pct_home,
               r->ast_home,
               r->reb_home,
               r->home_team_wins ? 1 : 0);
    }

    printf("===================\n\n");
    buffer_manager_unpin(bm, block);
}