#ifndef STATISTICS_H
#define STATISTICS_H

// number of index nodes accessed
extern int number_of_index_nodes_accessed;

// number of leaf nodes accessed
extern int number_of_leaf_nodes_accessed;

// number of unique data blocks accessed by B+ tree
extern int number_of_data_blocks_accessed_bp;

// number of data blocks accessed by linear scan
extern int number_of_data_blocks_accessed_l;

// number of games deleted 
extern int number_of_games_deleted;

// average of ft_pct_home
extern float ft_pct_home_avg;

// Total Running Time of retrieval process 
extern double retrieval_time_bp;
extern double running_time_bp;

// Total Running Time of linear scan
extern double running_time_linear;

// Lazy deletion phase timing variables
extern double lazy_pure_traversal_time;
extern double lazy_disk_marking_time;
extern double lazy_compaction_time;

void init_statistics(int capacity);
void cleanup_statistics(void);
void increment_data_block_access_count_bp(int block_id);
void increment_data_block_access_count_l(int block_id);
void increment_index_nodes_access_count();
void increment_leaf_nodes_access_count();

#endif
