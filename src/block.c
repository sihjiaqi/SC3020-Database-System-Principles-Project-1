#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <utils.h>
#include <statistics.h>
#include "block.h"
#include "storage.h"
#include <buffer_manager.h>

// delete records with ft_pct_home > 0.9
void deleteRecordsLinear(BufferManager *bm, int num_blocks) {
    if (!bm) return;

    startTimer();

    for (int i = 0; i < num_blocks; i++) {
        Block *blk = buffer_manager_get_block(bm, i);
        if (!blk) continue;

        increment_data_block_access_count_l(blk->block_id);

        for (int j = 0; j < blk->record_count; j++) {
            NBA_Record *rec = &blk->records[j];
            if (rec->ft_pct_home > 0.9f) {
                rec->is_valid = false;
                buffer_manager_mark_dirty(bm, blk);
            }
        }

        buffer_manager_unpin(bm, blk);
    }

    running_time_linear = getElapsedTimeMillis();
}