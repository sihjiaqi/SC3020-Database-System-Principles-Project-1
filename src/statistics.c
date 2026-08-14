#include "statistics.h"
#include <stdlib.h>
#include <stdbool.h>

int number_of_index_nodes_accessed = 0;
int number_of_leaf_nodes_accessed = 0;
int number_of_data_blocks_accessed_bp = 0;
int number_of_data_blocks_accessed_l = 0;
int number_of_games_deleted = 0;
float ft_pct_home_avg = 0;
double retrieval_time_bp = 0;
double running_time_bp = 0;
double running_time_linear = 0;

// Lazy deletion phase timing variables
double lazy_pure_traversal_time = 0;
double lazy_disk_marking_time = 0;
double lazy_compaction_time = 0;

// For B+ tree unique block counting
static int *seen_blocks_bp = NULL;
static int seen_count_bp = 0;
static int seen_capacity_bp = 0;

// For linear scan unique block counting
static int *seen_blocks_l = NULL;
static int seen_count_l = 0;
static int seen_capacity_l = 0;

// ---- Initialization ----
void init_statistics(int capacity) {
    number_of_data_blocks_accessed_bp = 0;
    number_of_data_blocks_accessed_l = 0;
    number_of_games_deleted = 0;
    ft_pct_home_avg = 0;
    running_time_bp = 0;
    retrieval_time_bp = 0;
    running_time_linear = 0;

    // B+ tree
    seen_capacity_bp = capacity;
    seen_blocks_bp = malloc(sizeof(int) * capacity);
    seen_count_bp = 0;

    // Linear scan
    seen_capacity_l = capacity;
    seen_blocks_l = malloc(sizeof(int) * capacity);
    seen_count_l = 0;
}

// ---- Cleanup ----
void cleanup_statistics(void) {
    free(seen_blocks_bp);
    seen_blocks_bp = NULL;
    seen_count_bp = 0;
    seen_capacity_bp = 0;

    free(seen_blocks_l);
    seen_blocks_l = NULL;
    seen_count_l = 0;
    seen_capacity_l = 0;
}

// ---- Helper for checking if a block has been seen ----
static bool already_seen_bp(int block_id) {
    for (int i = 0; i < seen_count_bp; i++) {
        if (seen_blocks_bp[i] == block_id) return true;
    }
    return false;
}

static bool already_seen_l(int block_id) {
    for (int i = 0; i < seen_count_l; i++) {
        if (seen_blocks_l[i] == block_id) return true;
    }
    return false;
}

// ---- Increment functions ----
void increment_data_block_access_count_bp(int block_id) {
    if (!already_seen_bp(block_id)) {
        if (seen_count_bp < seen_capacity_bp) {
            seen_blocks_bp[seen_count_bp++] = block_id;
        }
        number_of_data_blocks_accessed_bp++; // count unique blocks
    }
}

void increment_data_block_access_count_l(int block_id) {
    if (!already_seen_l(block_id)) {
        if (seen_count_l < seen_capacity_l) {
            seen_blocks_l[seen_count_l++] = block_id;
        }
        number_of_data_blocks_accessed_l++; // count unique blocks
    }
}

void increment_index_nodes_access_count() {
    number_of_index_nodes_accessed++;
}

void increment_leaf_nodes_access_count() {
    number_of_leaf_nodes_accessed++;
}