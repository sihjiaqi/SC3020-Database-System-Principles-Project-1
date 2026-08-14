#ifndef BPTREE_H
#define BPTREE_H

#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include "nba_record.h"
#include "block.h"
#include "statistics.h"
#include "storage.h"

extern int MAX_KEYS;

#pragma pack(push, 4)
typedef struct BPlusTreeNode
{
    float *keys;              // keys in this node
    NBA_Record_Pointer *data; // only for leaf nodes: points to record locations
    int numKeys;              // number of keys in the node
    bool isLeaf;              // true if this node is a leaf
    struct BPlusTreeNode **children; // child pointers for internal nodes
    struct BPlusTreeNode *next;      // next leaf for easy range queries
} BPlusTreeNode;

#pragma pack(pop)

typedef struct BPlusTree
{
    BPlusTreeNode *root;
} BPlusTree;

typedef struct
{
    bool valid;
    int leafDepth; // recorded depth of first leaf
} ValidationResult;

BPlusTreeNode *createLeafNode();
BPlusTreeNode *createNonLeafNode();
BPlusTreeNode *createNode(bool isLeaf);
int calculateMaxKeys(int block_size, bool isLeaf);
BPlusTree *createBPlusTree();

void freeNode(BPlusTreeNode *node);
void freeSubtree(BPlusTreeNode *node);
void freeBPlusTree(BPlusTree *tree);

BPlusTreeNode *findLeaf(BPlusTreeNode *root, float key);
BPlusTreeNode *findParent(BPlusTreeNode *root, BPlusTreeNode *child);

void insertInLeaf(BPlusTreeNode *leaf, float key, NBA_Record_Pointer value);
BPlusTreeNode *splitLeaf(BPlusTreeNode *leaf, float *newKey);
BPlusTreeNode *splitInternal(BPlusTreeNode *node, float *upKey);
void insertInParent(BPlusTree *tree, BPlusTreeNode *left, float key, BPlusTreeNode *right);
void insert(BPlusTree *tree, float key, NBA_Record_Pointer value);

int countLevels(BPlusTree *tree);
int countNodes(BPlusTreeNode *node);
int countLeafNodes(BPlusTree *tree);
void printTree(BPlusTreeNode *node, int level);
void printLastLeafNodeRecords(BufferManager *bm, BPlusTree *tree); // Print all records in the last leaf node
float *getRootKeys(BPlusTree *tree, int *numKeys);
float calculateLeafKeysAverage(BPlusTree *tree);
void printBPlusTreeStats(BPlusTree *tree);

void serializeNode(FILE *fp, BPlusTreeNode *node);
int saveBPlusTree(BPlusTree *tree, const char *filename);
void reconstructLeafLinks(BPlusTreeNode *node, BPlusTreeNode **lastLeaf);
BPlusTreeNode *deserializeNode(FILE *fp);
BPlusTree *loadBPlusTree(const char *filename);

void removeFromLeaf(BPlusTreeNode *leaf, int index);

bool canBorrowFromLeft(BPlusTreeNode *left, bool isLeaf);
bool canBorrowFromRight(BPlusTreeNode *right, bool isLeaf);
void borrowFromLeft(BPlusTreeNode *node, BPlusTreeNode *left, BPlusTreeNode *parent, int separationIdx);
void borrowFromRight(BPlusTreeNode *node, BPlusTreeNode *right, BPlusTreeNode *parent, int separationIdx);

void mergeWithLeft(BPlusTree *tree, BPlusTreeNode *node, BPlusTreeNode *left, BPlusTreeNode *parent, int separationIdx);
void mergeWithRight(BPlusTree *tree, BPlusTreeNode *node, BPlusTreeNode *right, BPlusTreeNode *parent, int separationIdx);
void rebalanceAfterDeletion(BPlusTree *tree, BPlusTreeNode *node);

bool deleteKey(BufferManager *bm, BPlusTree *tree, float key);
void deleteKeysAboveThreshold(BufferManager *bm, BPlusTree *tree, float threshold);
void verifyTreeStructure(BPlusTree *tree);
ValidationResult checkNode(BPlusTreeNode *node, int depth, int order, bool isRoot);
bool validateBPlusTree(BPlusTree *tree);

// Lazy deletion functions
int collectKeysForLazyDeletion(BPlusTree *tree, float threshold, float **keysToDelete, NBA_Record_Pointer **recordsToDelete, int *capacity);
int markCollectedKeysAsDeleted(BufferManager *bm, NBA_Record_Pointer *recordsToDelete, int count);
void lazyDeleteKeysAboveThreshold(BufferManager *bm, BPlusTree *tree, float threshold);
void strictRebalance(BPlusTree *tree);

// Comprehensive rebalancing functions
int getMinKeys(BPlusTreeNode *node, bool isLeaf);
bool fixUnderfullNodes(BPlusTree *tree, int minKeys);
bool fixUnderfullInternalNodes(BPlusTree *tree, int minKeys);
bool fixInternalNodesAtLevel(BPlusTree *tree, BPlusTreeNode *node, int currentLevel, int targetLevel, int minKeys, int *violationCount);
int getTreeHeight(BPlusTree *tree);
void removeEmptyNode(BPlusTree *tree, BPlusTreeNode *emptyNode);
void rebalanceUnderfullNode(BPlusTree *tree, BPlusTreeNode *underfullNode, int minKeys);

#endif
