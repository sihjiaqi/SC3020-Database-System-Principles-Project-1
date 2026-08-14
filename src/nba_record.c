#include "nba_record.h"
#include <stdlib.h>
#include <string.h>
#include "block.h"
#include "statistics.h"
#include "utils.h"

void printNBARecord(NBA_Record *record) {
    if (!record) {
        printf("NULL record\n");
        return;
    }

    int year, month, day;
    decodeDate(record->game_date_est, &year, &month, &day);

    printf("Game Date: %02d/%02d/%04d, Team ID: %d, Points: %d, "
           "FG%%: %.3f, FT%%: %.3f, 3FG%%: %.3f, Assists: %hd, "
           "Rebounds: %hd, Home Win: %d\n",
           day, month, year,
           record->team_id_home,
           record->pts_home,
           record->fg_pct_home,
           record->ft_pct_home,
           record->fg3_pct_home,
           record->ast_home,
           record->reb_home,
           record->home_team_wins ? 1 : 0);
}

bool markRecordAsDeleted(BufferManager *bm, NBA_Record_Pointer recordPtr) {
    if (!bm) return false;

    // Fetch block through buffer manager
    Block *block = buffer_manager_get_block(bm, recordPtr.block_id);
    if (!block) {
        printf("Error: Could not load block %d from buffer\n", recordPtr.block_id);
        return false;
    }

    // Sanity check: ensure record index is valid
    if (recordPtr.record_idx < 0 || recordPtr.record_idx >= block->record_count) {
        printf("Error: Invalid record_idx %d in block %d\n", 
               recordPtr.record_idx, recordPtr.block_id);
        buffer_manager_unpin(bm, block);
        return false;
    }

    NBA_Record *record = &block->records[recordPtr.record_idx];

    // Increment block access count (B+ tree path)
    // printf("Deleting record in block %d, index %d, with ft_pct_home value %f\n", recordPtr.block_id, recordPtr.record_idx, record->ft_pct_home);
    increment_data_block_access_count_bp(recordPtr.block_id);

    // Mark record as deleted
    bool deleted = false;
    if (record->is_valid) {
        record->is_valid = false;                  // logical delete
        buffer_manager_mark_dirty(bm, block);      // mark dirty in buffer
        block->timestamp = (uint32_t)time(NULL);   // update last modified
        deleted = true;
    }

    // Unpin block after usage
    buffer_manager_unpin(bm, block);

    return deleted;
}