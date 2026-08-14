#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "storage.h"
#include "statistics.h"
#include "block.h"
#include "nba_record.h"
#include "utils.h"
#include "b_plus_tree.h"
#include "buffer_manager.h"
#include <stdio.h>
#include <stdlib.h>

// File paths
#define NBA_TEXT_SOURCE "games.txt"
#define NBA_BINARY_DATA_BP_UC "binary/nba_data_bp_uc.bin"     // b plus tree, uncached
#define NBA_BINARY_DATA_BP_C "binary/nba_data_bp_c.bin"       // b plus tree, cached
#define NBA_BINARY_DATA_BP_LAZY "binary/nba_data_bp_lazy.bin" // b plus tree, lazy deletion
#define NBA_BINARY_DATA_LIN_UC "binary/nba_data_lin_uc.bin"   // linear, uncached
#define NBA_BINARY_DATA_LIN_C "binary/nba_data_lin_c.bin"     // linear, cached
// #define NBA_BINARY_DATA_POST_BP_DELETION "binary/nba_data_post_bp_deletion.bin"
// #define NBA_BINARY_DATA_POST_LINEAR_DELETION "binary/nba_data_post_linear_deletion.bin"

// B+ tree serialized files
#define BPTREE_ITERATIVE_FILE "binary/b_plus_tree_iterative.bin"
#define BPTREE_ITERATIVE_FILE_AFTER_DELETION "binary/b_plus_tree_iterative_after_deletion.bin"

// Comparator function to sort by FT_PCT_home in ascending order
int compare_ft_pct_home(const void *a, const void *b)
{
    const NBA_Record *rec_a = (const NBA_Record *)a;
    const NBA_Record *rec_b = (const NBA_Record *)b;

    if (rec_a->ft_pct_home < rec_b->ft_pct_home)
        return -1;
    else if (rec_a->ft_pct_home > rec_b->ft_pct_home)
        return 1;
    else
        return 0;
}

// Task 1: Store/load data to disk via buffer manager
int task1_store_data_to_disk_image(int block_size, BufferManager *bm)
{
    printf("================ TASK 1 ================\n");
    printf("\n--- TASK 1 Storage Statistics ---\n");

    // Read records from text
    NBA_Record *records = NULL;
    int num_records = 0;
    int result = read_data_from_file(NBA_TEXT_SOURCE, &records, &num_records);
    if (result != STORAGE_SUCCESS)
    {
        print_storage_error(result, "reading data from text file");
        return result;
    }

    for (int i = 0; i < num_records; i++)
        records[i].is_valid = true;

    qsort(records, num_records, sizeof(NBA_Record), compare_ft_pct_home);

    // Initialize buffer manager
    result = buffer_manager_init(bm, NBA_BINARY_DATA_BP_C, BUFFER_POOL_SIZE, block_size);
    if (result != STORAGE_SUCCESS)
    {
        print_storage_error(result, "initializing buffer manager");
        free(records);
        return result;
    }

    // Write records via buffer
    int block_id = 0;
    size_t block_header_size = offsetof(Block, records);
    int usable_space = block_size - (int)block_header_size;
    int records_per_block = usable_space / sizeof(NBA_Record);

    for (int i = 0; i < num_records; i += records_per_block)
    {
        int rec_count = (i + records_per_block > num_records) ? num_records - i : records_per_block;
        Block *blk = buffer_manager_get_block(bm, block_id);
        memset(blk, 0, block_size);
        blk->block_id = block_id;
        blk->record_count = rec_count;
        blk->timestamp = (uint32_t)time(NULL);
        memcpy(blk->records, &records[i], rec_count * sizeof(NBA_Record));
        buffer_manager_mark_dirty(bm, blk);
        buffer_manager_unpin(bm, blk);
        block_id++;
    }

    free(records);

    // Flush all dirty blocks
    buffer_manager_flush(bm);

    long file_size = getFileSize(NBA_BINARY_DATA_BP_C);
    int num_blocks = (int)(file_size / block_size);

    printf("+-------------------------+-------------------+\n");
    printf("| %-23s | %-17zu |\n", "Size of Record", sizeof(NBA_Record));
    printf("| %-23s | %-17d |\n", "Records per block", records_per_block);
    printf("| %-23s | %-17d |\n", "Total blocks", num_blocks);
    printf("| %-23s | %-17d |\n", "Total records", num_records);
    printf("+-------------------------+-------------------+\n\n");

    return STORAGE_SUCCESS;
}

// Task 2: Build B+ tree using buffer
int task2_build_b_plus_tree(int block_size, BufferManager *bm)
{
    printf("================ TASK 2 ================\n\n");

    BPlusTree *tree = createBPlusTree();

    long file_size = getFileSize(NBA_BINARY_DATA_BP_C);
    int num_blocks = (int)(file_size / block_size);

    for (int b = 0; b < num_blocks; b++)
    {
        Block *blk = buffer_manager_get_block(bm, b);
        if (!blk)
            continue;

        for (int r = 0; r < blk->record_count; r++)
        {
            NBA_Record *rec = &blk->records[r];
            if (rec->is_valid)
            {
                NBA_Record_Pointer ptr = {blk->block_id, r};
                insert(tree, rec->ft_pct_home, ptr);
            }
        }
        buffer_manager_unpin(bm, blk);
    }
    validateBPlusTree(tree);
    saveBPlusTree(tree, BPTREE_ITERATIVE_FILE);
    printBPlusTreeStats(tree);
    freeBPlusTree(tree);

    return STORAGE_SUCCESS;
}

// Task 3a: B+ tree deletions using buffer
int task3_perform_deletions_bp(int block_size, BufferManager *bm)
{
    printf("================ TASK 3 (B+ TREE DELETION) ================\n");

    BPlusTree *tree = loadBPlusTree(BPTREE_ITERATIVE_FILE);
    if (!tree)
        return STORAGE_ERROR_FILE_NOT_FOUND;

    deleteKeysAboveThreshold(bm, tree, 0.9f);
    validateBPlusTree(tree);
    saveBPlusTree(tree, BPTREE_ITERATIVE_FILE_AFTER_DELETION);

    buffer_manager_flush(bm);

    float pct_home_avg = calculateLeafKeysAverage(tree);
    printf("+-----------------------------------------+-----------------------+\n");
    printf("| %-39s | %21.3f |\n", "Traversal Time (ms)", retrieval_time_bp);
    printf("| %-39s | %21.3f |\n", "Deletion + Tree Rebalancing Time (ms)", running_time_bp);
    printf("| %-39s | %21.3f |\n", "Total Running Time (ms)", running_time_bp + retrieval_time_bp);
    printf("| %-39s | %21d |\n", "Number of internal nodes accessed", number_of_index_nodes_accessed);
    printf("| %-39s | %21d |\n", "Number of leaf nodes accessed", number_of_leaf_nodes_accessed);
    printf("| %-39s | %21d |\n", "Total number of nodes accessed", number_of_leaf_nodes_accessed + number_of_index_nodes_accessed);
    printf("| %-39s | %21d |\n", "Number of data blocks accessed", number_of_data_blocks_accessed_bp);
    printf("| %-39s | %21d |\n", "Number of games deleted", number_of_games_deleted);
    printf("| %-39s | %21.3f |\n", "Average of ft_pct_home", pct_home_avg);
    printf("+-----------------------------------------+-----------------------+\n\n");

    printBPlusTreeStats(tree);
    freeBPlusTree(tree);
    return STORAGE_SUCCESS;
}

// Task 3b (Lazy): B+ tree lazy deletions using buffer
int task3_perform_lazy_deletions_bp(int block_size, BufferManager *bm)
{
    printf("================ TASK 3 (B+ TREE LAZY DELETION) ================\n");

    BPlusTree *tree = loadBPlusTree(BPTREE_ITERATIVE_FILE);
    if (!tree)
        return STORAGE_ERROR_FILE_NOT_FOUND;

    lazyDeleteKeysAboveThreshold(bm, tree, 0.9f);
    validateBPlusTree(tree);
    saveBPlusTree(tree, BPTREE_ITERATIVE_FILE_AFTER_DELETION);

    buffer_manager_flush(bm);

    float pct_home_avg = calculateLeafKeysAverage(tree);
    printf("+---------------------------------------------+-----------------------+\n");
    printf("| %-43s | %21.3f |\n", "Phase 1: Pure Traversal Time (ms)", lazy_pure_traversal_time);
    printf("| %-43s | %21.3f |\n", "Phase 2: Disk Marking Time (ms)", lazy_disk_marking_time);
    printf("| %-43s | %21.3f |\n", "Phase 3: Compaction Time (ms)", lazy_compaction_time);
    printf("| %-43s | %21.3f |\n", "Total Running Time (ms)", lazy_pure_traversal_time + lazy_disk_marking_time + lazy_compaction_time);
    printf("| %-43s | %21d |\n", "Number of internal nodes accessed", number_of_index_nodes_accessed);
    printf("| %-43s | %21d |\n", "Number of leaf nodes accessed", number_of_leaf_nodes_accessed);
    printf("| %-43s | %21d |\n", "Total number of nodes accessed", number_of_leaf_nodes_accessed + number_of_index_nodes_accessed);
    printf("| %-43s | %21d |\n", "Number of data blocks accessed", number_of_data_blocks_accessed_bp);
    printf("| %-43s | %21d |\n", "Number of games deleted", number_of_games_deleted);
    printf("| %-43s | %21.3f |\n", "Average of ft_pct_home", pct_home_avg);
    printf("+---------------------------------------------+-----------------------+\n");

    // printLastLeafNodeRecords(bm, tree);
    printBPlusTreeStats(tree);
    freeBPlusTree(tree);
    return STORAGE_SUCCESS;
}

// Task 3c: Linear scan deletions using buffer
int task3_perform_deletions_linear(BufferManager *bm)
{
    printf("================TASK3 LINEAR SCAN================\n");
    long file_size = getFileSize(NBA_BINARY_DATA_BP_C);
    int num_blocks = (int)(file_size / bm->block_size);

    deleteRecordsLinear(bm, num_blocks);
    buffer_manager_flush(bm);

    printf("+-------------------------+--------------------------+\n");
    printf("| %-30s | %17.3f |\n", "Total Running Time (ms)", running_time_linear);
    printf("| %-30s | %17d |\n", "Number of data blocks accessed", number_of_data_blocks_accessed_l);
    printf("+-------------------------+--------------------------+\n\n");

    return STORAGE_SUCCESS;
}

int copy_file(const char *src, const char *dst)
{
    FILE *fsrc = fopen(src, "rb");
    if (!fsrc)
        return 0;

    FILE *fdst = fopen(dst, "wb");
    if (!fdst)
    {
        fclose(fsrc);
        return 0;
    }

    char buffer[8192];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fsrc)) > 0)
    {
        fwrite(buffer, 1, bytes, fdst);
    }

    fclose(fsrc);
    fclose(fdst);
    return 1;
}

int main()
{
    g_block_size = get_block_size();

    // ---------------- Task 1 ----------------
    BufferManager bm_task1;
    if (task1_store_data_to_disk_image(g_block_size, &bm_task1) != STORAGE_SUCCESS)
        return 1;

    // ---------------- Task 2 ----------------
    BufferManager bm_task2;
    buffer_manager_init(&bm_task2, NBA_BINARY_DATA_BP_C, BUFFER_POOL_SIZE, g_block_size);
    task2_build_b_plus_tree(g_block_size, &bm_task2);
    buffer_manager_shutdown(&bm_task2);

    // Make a copy for linear scan so it has original data
    if (!copy_file(NBA_BINARY_DATA_BP_C, NBA_BINARY_DATA_LIN_C))
    {
        fprintf(stderr, "Error copying file for linear scan\n");
        return 1;
    }
    if (!copy_file(NBA_BINARY_DATA_BP_C, NBA_BINARY_DATA_BP_LAZY))
    {
        fprintf(stderr, "Error copying file for linear scan\n");
        return 1;
    }

    init_statistics(290);
    // cached
    // printf("================= CACHED ================\n\n");
    // ---------------- Task 3a (B+ tree) ----------------
    BufferManager bm_task3_bp_cached;
    buffer_manager_init(&bm_task3_bp_cached, NBA_BINARY_DATA_BP_C, BUFFER_POOL_SIZE, g_block_size);
    task3_perform_deletions_bp(g_block_size, &bm_task3_bp_cached);
    buffer_manager_shutdown(&bm_task3_bp_cached);

    // ---------------- Task 3b (B+ tree LAZY) ----------------
    // Run lazy deletion on ORIGINAL unmodified data
    BufferManager bm_task3_bp_lazy;
    buffer_manager_init(&bm_task3_bp_lazy, NBA_BINARY_DATA_BP_LAZY, BUFFER_POOL_SIZE, g_block_size);
    task3_perform_lazy_deletions_bp(g_block_size, &bm_task3_bp_lazy);
    buffer_manager_shutdown(&bm_task3_bp_lazy);

    // ---------------- Task 3c (Linear) ----------------
    BufferManager bm_task3_lin_cached;
    buffer_manager_init(&bm_task3_lin_cached, NBA_BINARY_DATA_LIN_C, BUFFER_POOL_SIZE, g_block_size);
    task3_perform_deletions_linear(&bm_task3_lin_cached);
    buffer_manager_shutdown(&bm_task3_lin_cached);

    return 0;
}
