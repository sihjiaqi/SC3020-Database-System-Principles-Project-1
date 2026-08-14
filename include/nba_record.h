#ifndef NBA_RECORD_H
#define NBA_RECORD_H

#include <stdbool.h>
#include "buffer_manager.h"

// Optimize record structure with 4-byte alignment
#pragma pack(push, 4)

typedef struct NBA_Record {
    bool is_valid; // true if the record has not been deleted

    int team_id_home;
    int pts_home;
    float fg_pct_home;
    float ft_pct_home;
    float fg3_pct_home;

    short ast_home;
    short reb_home;

    bool home_team_wins;

    unsigned char game_date_est[3]; // packed: year(12b), month(4b), day(5b)
} NBA_Record;

#pragma pack(pop)

typedef struct NBA_Record_Pointer {
    int block_id;    // which block the record is in
    int record_idx;  // index of the record within the block
} NBA_Record_Pointer;

// Print a single NBA record
void printNBARecord(NBA_Record *record);

// mark record as deleted
bool markRecordAsDeleted(BufferManager *bm, NBA_Record_Pointer recordPtr);

#endif // NBA_RECORD_H