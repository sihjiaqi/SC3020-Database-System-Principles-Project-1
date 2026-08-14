// b_plus_tree.c
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "nba_record.h"
#include "block.h"
#include "b_plus_tree.h"
#include "storage.h"
#include "statistics.h"
#include "utils.h"

int MAX_KEYS;

// ======= NODE CREATION =======

// method for creating a leaf node
BPlusTreeNode *createLeafNode()
{
    BPlusTreeNode *node = (BPlusTreeNode *)malloc(sizeof(BPlusTreeNode));
    if (!node)
        return NULL;

    node->keys = (float *)calloc(MAX_KEYS + 1, sizeof(float)); // +1 for overflow during insert
    node->data = (NBA_Record_Pointer *)calloc(MAX_KEYS + 1, sizeof(NBA_Record_Pointer));

    if (!node->keys || !node->data)
    {
        free(node->keys);
        free(node->data);
        free(node);
        return NULL;
    }

    node->isLeaf = true;
    // node->is_dirty = false;
    node->children = NULL;
    node->numKeys = 0;
    node->next = NULL;
    return node;
}

// method for creating a non leaf node
BPlusTreeNode *createNonLeafNode()
{
    BPlusTreeNode *node = (BPlusTreeNode *)malloc(sizeof(BPlusTreeNode));
    if (!node)
        return NULL;

    node->keys = (float *)calloc(MAX_KEYS + 1, sizeof(float));                        // +1 for overflow during insert
    node->children = (BPlusTreeNode **)calloc(MAX_KEYS + 2, sizeof(BPlusTreeNode *)); // +1 child

    if (!node->keys || !node->children)
    {
        free(node->keys);
        free(node->children);
        free(node);
        return NULL;
    }

    node->isLeaf = false;
    // node->is_dirty = false;
    node->data = NULL;
    node->numKeys = 0;
    node->next = NULL;
    return node;
}

BPlusTreeNode *createNode(bool isLeaf)
{
    return isLeaf ? createLeafNode() : createNonLeafNode();
}

int calculateMaxKeys(int block_size, bool isLeaf)
{
    int fixed_size = sizeof(void *) * 4 + sizeof(int) + sizeof(bool); // rough
    int per_key_size = isLeaf ? (sizeof(float) + sizeof(NBA_Record_Pointer)) : (sizeof(float) + sizeof(void *));
    if (block_size <= fixed_size)
        return 0;
    return (block_size - fixed_size) / per_key_size;
}

BPlusTree *createBPlusTree()
{
    // set max keys
    MAX_KEYS = calculateMaxKeys(g_block_size, 0);
    printf("Calculated max keys per node: %d\n", MAX_KEYS);

    BPlusTree *tree = (BPlusTree *)malloc(sizeof(BPlusTree));
    if (!tree)
        return NULL;
    tree->root = createLeafNode();
    if (!tree->root)
    {
        free(tree);
        return NULL;
    }
    return tree;
}

// ====== NODE CLEANUP =====
void freeNode(BPlusTreeNode *node)
{
    if (!node)
        return;

    free(node->keys);
    node->keys = NULL;

    if (node->data)
    {
        free(node->data);
        node->data = NULL;
    }

    if (node->children)
    {
        free(node->children);
        node->children = NULL;
    }

    free(node);
}

// Free all nodes recursively
void freeSubtree(BPlusTreeNode *node)
{
    if (!node)
        return;

    if (!node->isLeaf)
    {
        for (int i = 0; i <= node->numKeys; i++)
        {
            freeSubtree(node->children[i]);
        }
    }
    freeNode(node);
}

void freeBPlusTree(BPlusTree *tree)
{
    if (!tree)
        return;

    freeSubtree(tree->root);
    free(tree);
}

// ============== FINDING METHODS

BPlusTreeNode *findLeaf(BPlusTreeNode *root, float key)
{
    if (!root)
        return NULL;
    BPlusTreeNode *cur = root;
    while (!cur->isLeaf)
    {
        increment_index_nodes_access_count();
        int i = 0;
        // linear scan to find first key > key, follow children[i]
        while (i < cur->numKeys && key >= cur->keys[i])
            i++;
        cur = cur->children[i];
        if (!cur)
            return NULL;
    }
    increment_leaf_nodes_access_count();
    return cur;
}

BPlusTreeNode *findParent(BPlusTreeNode *root, BPlusTreeNode *child)
{
    if (!root || !child || root->isLeaf || root == child)
        return NULL;

    increment_index_nodes_access_count();

    for (int i = 0; i <= root->numKeys; i++)
    {
        if (root->children[i] == child)
            return root;
        if (root->children[i] && !root->children[i]->isLeaf)
        {
            BPlusTreeNode *p = findParent(root->children[i], child);
            if (p)
                return p;
        }
    }
    return NULL;
}

// =========== INSERT METHODS

void insertInLeaf(BPlusTreeNode *leaf, float key, NBA_Record_Pointer value)
{
    // this method assumes that there is no overflow during insertion
    // a check is done before itself by the method calling this
    if (!leaf || !leaf->isLeaf)
        return;

    int i = leaf->numKeys - 1;
    while (i >= 0 && leaf->keys[i] > key)
    {
        leaf->keys[i + 1] = leaf->keys[i];
        leaf->data[i + 1] = leaf->data[i];
        i--;
    }
    leaf->keys[i + 1] = key;
    leaf->data[i + 1] = value;
    leaf->numKeys++;
    // leaf->is_dirty = true;
}

BPlusTreeNode *splitLeaf(BPlusTreeNode *leaf, float *newKey)
{
    if (!leaf || !leaf->isLeaf || leaf->numKeys <= MAX_KEYS)
        return NULL;

    int total = leaf->numKeys;
    int split = (total + 1) / 2; // left keeps 'split' keys
    // floor((total+1)/2) stays in the current node
    // ceil((total+1)/2) moves to the next (new) node

    BPlusTreeNode *newLeaf = createLeafNode();
    if (!newLeaf)
        return NULL;

    int moving = total - split;
    // Copy the right-side keys/data into newLeaf
    for (int i = 0; i < moving; i++)
    {
        newLeaf->keys[i] = leaf->keys[split + i];
        newLeaf->data[i] = leaf->data[split + i];
    }
    newLeaf->numKeys = moving;
    leaf->numKeys = split;

    // link leaves
    newLeaf->next = leaf->next;
    leaf->next = newLeaf;

    // newLeaf->is_dirty = true;
    // leaf->is_dirty = true;

    // get the first key of the new node to insert to the parent
    *newKey = newLeaf->keys[0];
    return newLeaf;
}

BPlusTreeNode *splitInternal(BPlusTreeNode *node, float *upKey)
{
    if (!node || node->isLeaf || node->numKeys <= MAX_KEYS)
        return NULL;

    int total = node->numKeys;
    int mid = total / 2;
    // the keys till the upkey stay in the current node
    // the upkey moves to the parent node
    // the keys after the upkey move to a new node which is at the same level as the current node
    *upKey = node->keys[mid];

    BPlusTreeNode *newNode = createNonLeafNode();
    if (!newNode)
        return NULL;

    int newCount = total - mid - 1; // -1 for the key you are sending up
    // Copy keys to new node
    for (int i = 0; i < newCount; i++)
    {
        newNode->keys[i] = node->keys[mid + 1 + i];         // copy keys
        newNode->children[i] = node->children[mid + 1 + i]; // copy children
    }

    // copy over the last remaining pointer
    newNode->children[newCount] = node->children[total];

    // update key counts
    newNode->numKeys = newCount;
    node->numKeys = mid; // Shrink original node

    // newNode->is_dirty = true;
    // node->is_dirty = true;

    return newNode;
}

void insertInParent(BPlusTree *tree, BPlusTreeNode *left, float key, BPlusTreeNode *right)
{
    // left is the node that just got split
    // right is the new node that got created
    // basically we want to insert the upkey into the parent
    // the pointer after the upkey must point to the new node
    if (!tree || !left || !right)
        return;

    // If left was root -> new root
    if (tree->root == left)
    {
        BPlusTreeNode *newRoot = createNonLeafNode();
        if (!newRoot)
            return;
        newRoot->keys[0] = key;
        newRoot->children[0] = left;
        newRoot->children[1] = right;
        newRoot->numKeys = 1;
        // newRoot->is_dirty = true;
        tree->root = newRoot;
        return;
    }

    // otherwise find the parent of left
    BPlusTreeNode *parent = findParent(tree->root, left);

    // insert key and right child into parent
    int i = parent->numKeys - 1;
    while (i >= 0 && parent->keys[i] > key)
    {
        // shift all keys and children pointers to the right to
        // make space for the new key
        parent->keys[i + 1] = parent->keys[i];
        parent->children[i + 2] = parent->children[i + 1];
        i--;
    }
    // insert the new key into the space created
    // the pointer after the new key will be the
    // pointer to the right node.
    parent->keys[i + 1] = key;
    parent->children[i + 2] = right;
    parent->numKeys++;
    // parent->is_dirty = true;

    // If overflow, split internal node
    if (parent->numKeys > MAX_KEYS)
    {
        float upKey;
        BPlusTreeNode *newInternal = splitInternal(parent, &upKey);
        if (newInternal)
        {
            // insert the new node into the parent and repeat this process
            // until overflow stops
            insertInParent(tree, parent, upKey, newInternal);
        }
    }
}

void insert(BPlusTree *tree, float key, NBA_Record_Pointer value)
{
    if (!tree)
        return;
    if (!tree->root)
    {
        tree->root = createLeafNode();
        if (!tree->root)
            return;
    }

    BPlusTreeNode *leaf = findLeaf(tree->root, key);
    if (!leaf)
        return;

    insertInLeaf(leaf, key, value);

    if (leaf->numKeys > MAX_KEYS)
    {
        float newKey;
        BPlusTreeNode *newLeaf = splitLeaf(leaf, &newKey);
        if (newLeaf)
        {
            insertInParent(tree, leaf, newKey, newLeaf);
        }
    }
}

// ============ UTILITY

int countLevels(BPlusTree *tree)
{
    // since the tree is always balanced, it
    // walks down the leftmost path --> this
    // ends up being the height of the tree
    if (!tree || !tree->root)
        return 0;
    int levels = 0;
    BPlusTreeNode *cur = tree->root;
    while (cur)
    {
        levels++;
        if (cur->isLeaf)
            break;
        cur = cur->children[0];
    }
    return levels;
}

int countNodes(BPlusTreeNode *node)
{
    if (!node)
        return 0;
    int cnt = 1;
    if (!node->isLeaf)
    {
        for (int i = 0; i <= node->numKeys; i++)
        {
            if (node->children[i])
                cnt += countNodes(node->children[i]);
        }
    }
    return cnt;
}

int countLeafNodes(BPlusTree *tree)
{
    if (!tree || !tree->root)
        return 0;

    // Find leftmost leaf
    BPlusTreeNode *leaf = tree->root;
    while (!leaf->isLeaf)
    {
        leaf = leaf->children[0];
    }
    int count = 0;
    while (leaf)
    {
        count++;
        leaf = leaf->next;
    }

    return count;
}

float *getRootKeys(BPlusTree *tree, int *numKeys)
{
    if (!numKeys)
    {
        return NULL;
    }
    *numKeys = 0;
    if (!tree || !tree->root)
        return NULL;
    BPlusTreeNode *root = tree->root;
    *numKeys = root->numKeys;
    if (*numKeys == 0)
    {
        return NULL;
    }
    float *arr = (float *)malloc((*numKeys) * sizeof(float));
    if (!arr)
    {
        *numKeys = 0;
        return NULL;
    }
    for (int i = 0; i < *numKeys; i++)
    {
        arr[i] = root->keys[i];
    }
    return arr;
}

void printTree(BPlusTreeNode *node, int level)
{
    if (!node)
        return;

    printf("Level %d | %s | Keys: ", level, node->isLeaf ? "Leaf" : "Internal");
    for (int i = 0; i < node->numKeys; i++)
        printf("%.3f ", node->keys[i]);
    printf("\n");

    if (!node->isLeaf)
    {
        for (int i = 0; i <= node->numKeys; i++)
            printTree(node->children[i], level + 1);
    }
}

float calculateLeafKeysAverage(BPlusTree *tree)
{
    if (!tree || !tree->root)
        return 0.0f;

    // Find the leftmost leaf node
    BPlusTreeNode *leaf = tree->root;
    while (!leaf->isLeaf)
    {
        leaf = leaf->children[0];
    }

    float sum = 0.0f;
    int totalKeys = 0;

    // Traverse all leaf nodes using the linked list
    while (leaf)
    {
        for (int i = 0; i < leaf->numKeys; i++)
        {
            sum += leaf->keys[i];
            totalKeys++;
        }
        leaf = leaf->next;
    }

    if (totalKeys == 0)
        return 0.0f;
    // printf("Total number of leaf nodes : %d, average of all keys in leaf node : %d \n", totalKeys, sum/totalKeys);
    return sum / totalKeys;
}

void printBPlusTreeStats(BPlusTree *tree)
{
    if (!tree)
        return;

    int numNodes = countNodes(tree->root);
    int numLevels = countLevels(tree);
    float avgLeafKeys = calculateLeafKeysAverage(tree);

    int numKeys;
    float *keys = getRootKeys(tree, &numKeys);

    const int col1Width = 29;
    const int col2Width = 25;                         // Reduced slightly for better fit
    const int totalWidth = col1Width + col2Width + 5; // +5 for borders and spaces

    // Print header
    printf("+");
    for (int i = 0; i < col1Width; i++)
        printf("-");
    printf("+");
    for (int i = 0; i < col2Width; i++)
        printf("-");
    printf("+\n");

    // Print basic stats
    printf("| %-*s | %-*d |\n", col1Width - 1, "Max keys per node (n)", col2Width - 1, MAX_KEYS);
    printf("| %-*s | %-*d |\n", col1Width - 1, "Number of nodes", col2Width - 1, numNodes);
    printf("| %-*s | %-*d |\n", col1Width - 1, "Number of levels", col2Width - 1, numLevels);
    printf("| %-*s | %-*.3f |\n", col1Width - 1, "Average of leaf keys", col2Width - 1, avgLeafKeys);

    // Handle root node keys
    printf("| %-*s | ", col1Width - 1, "Root node keys");

    if (keys && numKeys > 0)
    {
        // Calculate how much space we have for keys on the first line
        int availableWidth = col2Width - 1;
        int currentPos = 0;
        bool isFirstLine = true;

        for (int i = 0; i < numKeys; i++)
        {
            char keyStr[16];
            int keyLen = snprintf(keyStr, sizeof(keyStr), "%.3f", keys[i]);

            // Add space before key (except for first key on a line)
            int totalLen = keyLen + (currentPos > 0 ? 1 : 0);

            // Check if this key fits on current line
            if (currentPos + totalLen > availableWidth)
            {
                // Pad current line and start new line
                for (int j = currentPos; j < availableWidth; j++)
                    printf(" ");
                printf(" |\n");
                printf("| %-*s | ", col1Width - 1, ""); // Empty left column for continuation
                currentPos = 0;
                isFirstLine = false;
            }

            // Print space before key (except first key on line)
            if (currentPos > 0)
            {
                printf(" ");
                currentPos++;
            }

            // Print the key
            printf("%s", keyStr);
            currentPos += keyLen;
        }

        // Pad the final line
        for (int j = currentPos; j < availableWidth; j++)
            printf(" ");
        printf(" |\n");

        free(keys);
    }
    else
    {
        printf("%-*s |\n", col2Width - 1, "(empty root)");
    }

    // Print footer
    printf("+");
    for (int i = 0; i < col1Width; i++)
        printf("-");
    printf("+");
    for (int i = 0; i < col2Width; i++)
        printf("-");
    printf("+\n\n");
}

// Print all records in the last leaf node of the B+ tree
void printLastLeafNodeRecords(BufferManager *bm, BPlusTree *tree)
{
    if (!tree || !tree->root)
    {
        printf("Tree is empty\n");
        return;
    }

    // Find the leftmost leaf node first
    BPlusTreeNode *leaf = tree->root;
    while (!leaf->isLeaf)
    {
        leaf = leaf->children[0];
    }

    // Traverse to the last leaf node using the linked list
    BPlusTreeNode *lastLeaf = leaf;
    while (lastLeaf->next)
    {
        lastLeaf = lastLeaf->next;
    }

    printf("================ LAST LEAF NODE RECORDS ================\n");
    printf("Last leaf node has %d records:\n\n", lastLeaf->numKeys);

    if (lastLeaf->numKeys == 0)
    {
        printf("No records in the last leaf node.\n");
        printf("=======================================================\n\n");
        return;
    }

    // Print header
    printf("%-5s | %-12s | %-10s | %-10s | %-10s | %-10s | %-10s | %-10s | %-10s\n",
           "Index", "FT_PCT_HOME", "Date", "Team_ID", "PTS_Home", "FG_PCT", "FG3_PCT", "AST", "REB", "Win");
    printf("------|--------------|------------|----------|----------|----------|----------|----------|----------|------\n");

    for (int i = 0; i < lastLeaf->numKeys; i++)
    {
        NBA_Record_Pointer ptr = lastLeaf->data[i];

        // Get the actual record from disk using buffer manager
        Block *block = buffer_manager_get_block(bm, ptr.block_id);
        if (!block)
        {
            printf("%-5d | %-12.3f | ERROR: Could not read block %d\n", i, lastLeaf->keys[i], ptr.block_id);
            continue;
        }

        if (ptr.record_idx >= block->record_count)
        {
            printf("%-5d | %-12.3f | ERROR: Invalid record index %d (block has %d records)\n",
                   i, lastLeaf->keys[i], ptr.record_idx, block->record_count);
            buffer_manager_unpin(bm, block);
            continue;
        }

        NBA_Record *record = &block->records[ptr.record_idx];

        // Check if record is valid
        if (!record->is_valid)
        {
            printf("%-5d | %-12.3f | [DELETED] Record marked as invalid\n", i, lastLeaf->keys[i]);
            buffer_manager_unpin(bm, block);
            continue;
        }

        // Decode date
        int year, month, day;
        decodeDate(record->game_date_est, &year, &month, &day);

        // Print record details
        printf("%-5d | %-12.3f | %02d/%02d/%04d | %-8d | %-8d | %-8.3f | %-8.3f | %-8d | %-8d | %-4s\n",
               i,
               lastLeaf->keys[i],
               day, month, year,
               record->team_id_home,
               record->pts_home,
               record->fg_pct_home,
               record->fg3_pct_home,
               record->ast_home,
               record->reb_home,
               record->home_team_wins ? "Yes" : "No");

        buffer_manager_unpin(bm, block);
    }

    printf("=======================================================\n\n");
}

// ================== SERIALIZATION

void serializeNode(FILE *fp, BPlusTreeNode *node)
{
    if (!fp || !node)
        return;

    fwrite(&node->isLeaf, sizeof(bool), 1, fp);
    fwrite(&node->numKeys, sizeof(int), 1, fp);

    if (node->numKeys > 0)
        fwrite(node->keys, sizeof(float), node->numKeys, fp);

    if (node->isLeaf)
    {
        if (node->numKeys > 0)
            fwrite(node->data, sizeof(NBA_Record_Pointer), node->numKeys, fp);
    }
    else
    {
        for (int i = 0; i <= node->numKeys; i++)
        {
            serializeNode(fp, node->children[i]);
        }
    }
}

int saveBPlusTree(BPlusTree *tree, const char *filename)
{
    if (!tree || !tree->root || !filename)
        return STORAGE_ERROR_INVALID_PARAMETER;
    FILE *fp = fopen(filename, "wb");
    if (!fp)
        return STORAGE_ERROR_FILE_NOT_FOUND;

    serializeNode(fp, tree->root);
    fclose(fp);
    return STORAGE_SUCCESS;
}

BPlusTreeNode *deserializeNode(FILE *fp)
{
    if (!fp)
        return NULL;

    bool isLeaf;
    int numKeys;

    if (fread(&isLeaf, sizeof(bool), 1, fp) != 1)
        return NULL;
    if (fread(&numKeys, sizeof(int), 1, fp) != 1)
        return NULL;
    if (numKeys < 0 || numKeys > MAX_KEYS + 1)
        return NULL;

    BPlusTreeNode *node = isLeaf ? createLeafNode() : createNonLeafNode();
    if (!node)
        return NULL;

    node->numKeys = numKeys;

    if (numKeys > 0)
    {
        if (fread(node->keys, sizeof(float), numKeys, fp) != (size_t)numKeys)
        {
            freeNode(node);
            return NULL;
        }
    }

    if (isLeaf)
    {
        if (numKeys > 0)
        {
            if (fread(node->data, sizeof(NBA_Record_Pointer), numKeys, fp) != (size_t)numKeys)
            {
                freeNode(node);
                return NULL;
            }
        }
    }
    else
    {
        for (int i = 0; i <= numKeys; i++)
        {
            node->children[i] = deserializeNode(fp);
            if (!node->children[i])
            {
                freeNode(node);
                return NULL;
            }
        }
    }

    return node;
}

// After the tree is deserialized, fix the leaf linked list
void reconstructLeafLinks(BPlusTreeNode *node, BPlusTreeNode **lastLeaf)
{
    if (!node)
        return;

    if (node->isLeaf)
    {
        if (*lastLeaf)
        {
            (*lastLeaf)->next = node;
        }
        *lastLeaf = node;
    }
    else
    {
        for (int i = 0; i <= node->numKeys; i++)
        {
            reconstructLeafLinks(node->children[i], lastLeaf);
        }
    }
}

BPlusTree *loadBPlusTree(const char *filename)
{
    if (!filename)
        return NULL;
    FILE *fp = fopen(filename, "rb");
    if (!fp)
        return NULL;

    BPlusTree *tree = (BPlusTree *)malloc(sizeof(BPlusTree));
    if (!tree)
    {
        fclose(fp);
        return NULL;
    }

    tree->root = deserializeNode(fp);
    fclose(fp);

    if (!tree->root)
    {
        free(tree);
        return NULL;
    }

    // Reconstruct leaf linked list
    BPlusTreeNode *lastLeaf = NULL;
    reconstructLeafLinks(tree->root, &lastLeaf);

    return tree;
}

// ============= DELETION

void removeFromLeaf(BPlusTreeNode *leaf, int index)
{
    // this method is simply for deleting from the leaf
    // there is no underflow check yet
    if (!leaf || !leaf->isLeaf || index < 0 || index >= leaf->numKeys)
        return;

    // move all the remaininf keys to the left
    // this ensures that all the empty spaces in the node are
    // in the right of the node
    for (int i = index; i < leaf->numKeys - 1; i++)
    {
        leaf->keys[i] = leaf->keys[i + 1];
        leaf->data[i] = leaf->data[i + 1];
    }
    leaf->numKeys--;
    // leaf->is_dirty = true;
}

bool canBorrowFromLeft(BPlusTreeNode *left, bool isLeaf)
{
    // check if left node exists
    // if not, then cannot borrow from left node
    if (!left)
        return false;

    int minKeys = 0;
    if (isLeaf)
    {
        minKeys = (MAX_KEYS + 1) / 2;
    }
    else
    {
        // if not leaf node
        minKeys = (MAX_KEYS) / 2;
    }

    // can borrow from left if left has more keys than the minimum
    bool canBorrow = left->numKeys > minKeys;
    return canBorrow;
}

bool canBorrowFromRight(BPlusTreeNode *right, bool isLeaf)
{
    if (!right)
        return false;

    int minKeys = 0;
    if (isLeaf)
    {
        minKeys = (MAX_KEYS + 1) / 2;
    }
    else
    {
        // if not leaf node
        minKeys = (MAX_KEYS) / 2;
    }

    // can borrow from right if right has more keys than the minimum
    bool canBorrow = right->numKeys > minKeys;
    return canBorrow;
}

void borrowFromLeft(BPlusTreeNode *node, BPlusTreeNode *left, BPlusTreeNode *parent, int separationIdx)
{
    // node is the node that is borrowing from the left node
    // left is the node that is giving its rightmost key
    // parent is the parent of the node and the left --> !! borrowing can only be
    // done between siblings that is they must have the same parent node
    // seperationIdx is the index where the key of the old node is
    if (!node || !left || !parent || separationIdx < 0)
        return;

    if (node->isLeaf)
    {
        // =============== BORROWING FROM LEAF NODE =========

        // Shift all keys/data in node to the right
        for (int i = node->numKeys; i > 0; i--)
        {
            node->keys[i] = node->keys[i - 1];
            node->data[i] = node->data[i - 1];
        }
        // Move the last key/data from left to node
        node->keys[0] = left->keys[left->numKeys - 1];
        node->data[0] = left->data[left->numKeys - 1];

        node->numKeys++;
        left->numKeys--;

        // Update parent separator
        parent->keys[separationIdx] = node->keys[0];
    }
    else
    {
        // =============== INTERNAL NODE BORROWING ==========

        // Shift keys and children in node to the right to make
        // space for the new key
        for (int i = node->numKeys; i > 0; i--)
        {
            node->keys[i] = node->keys[i - 1];
        }
        // if internal node then will have 1 more pointer than
        // leaf node --> thats why we need to use 2 separate loops

        for (int i = node->numKeys + 1; i > 0; i--)
        {
            node->children[i] = node->children[i - 1];
        }

        // Move separator from parent to node
        node->keys[0] = parent->keys[separationIdx];

        // Move last child from left to node
        node->children[0] = left->children[left->numKeys];

        // Move last key from left to parent
        parent->keys[separationIdx] = left->keys[left->numKeys - 1];

        node->numKeys++;
        left->numKeys--;
    }

    // node->is_dirty = true;
    // left->is_dirty = true;
    // parent->is_dirty = true;
}

void borrowFromRight(BPlusTreeNode *node, BPlusTreeNode *right, BPlusTreeNode *parent, int separationIdx)
{
    if (!node || !right || !parent || separationIdx >= parent->numKeys)
        return;

    if (node->isLeaf)
    {
        // Move first key/data from right to end of node
        node->keys[node->numKeys] = right->keys[0];
        node->data[node->numKeys] = right->data[0];
        node->numKeys++;

        // Shift everything in right to the left
        for (int i = 0; i < right->numKeys - 1; i++)
        {
            right->keys[i] = right->keys[i + 1];
            right->data[i] = right->data[i + 1];
        }
        right->numKeys--;

        // Update parent separator
        parent->keys[separationIdx] = right->keys[0];
    }
    else
    {
        // Move separator from parent to node
        node->keys[node->numKeys] = parent->keys[separationIdx];

        // Move first child from right to node
        node->children[node->numKeys + 1] = right->children[0];
        node->numKeys++;

        // Move first key from right to parent
        parent->keys[separationIdx] = right->keys[0];

        // Shift keys and children in right
        for (int i = 0; i < right->numKeys - 1; i++)
        {
            right->keys[i] = right->keys[i + 1];
        }
        for (int i = 0; i < right->numKeys; i++)
        {
            right->children[i] = right->children[i + 1];
        }

        right->numKeys--;
    }

    // node->is_dirty = true;
    // right->is_dirty = true;
    // parent->is_dirty = true;
}

void mergeWithLeft(BPlusTree *tree, BPlusTreeNode *node, BPlusTreeNode *left,
                   BPlusTreeNode *parent, int separationIdx)
{
    // printf("Calling merge with left\n");
    // node is the underflowing node
    // left is the node we are merging with
    // parent is the mutual parent
    // separation index is the index of the key in parent that separates left and node

    if (!tree || !node || !left || !parent || separationIdx < 0)
        return;

    if (node->isLeaf)
    {
        // Copy all keys/data from node into left
        for (int i = 0; i < node->numKeys; i++)
        {
            left->keys[left->numKeys + i] = node->keys[i];
            left->data[left->numKeys + i] = node->data[i];
        }
        left->numKeys += node->numKeys;

        // CRITICAL: Update the linked list to skip the deleted node
        left->next = node->next;
    }
    else
    {
        // For internal nodes: bring down the separator key from parent
        left->keys[left->numKeys] = parent->keys[separationIdx];
        left->children[left->numKeys + 1] = node->children[0];
        left->numKeys++;

        // Copy all keys and children from node to left
        int childIdx = left->numKeys + 1;
        for (int i = 0; i < node->numKeys; i++)
        {
            left->keys[left->numKeys] = node->keys[i];
            left->children[childIdx++] = node->children[i + 1];
            left->numKeys++;
        }
    }

    // Remove the separator key from parent and shift everything left
    for (int i = separationIdx; i < parent->numKeys - 1; i++)
    {
        parent->keys[i] = parent->keys[i + 1];
        parent->children[i + 1] = parent->children[i + 2];
    }
    parent->numKeys--;

    // FREE THE MERGED NODE - this is where the actual deletion happens!
    freeNode(node);

    // Handle root case - if parent becomes empty
    if (parent == tree->root && parent->numKeys == 0)
    {
        tree->root = left;
        freeNode(parent);
        return;
    }

    // Check if parent needs rebalancing
    int minKeys = parent->isLeaf ? (MAX_KEYS + 1) / 2 : (MAX_KEYS) / 2;
    if (parent->numKeys < minKeys)
    {
        rebalanceAfterDeletion(tree, parent);
    }
}

void mergeWithRight(BPlusTree *tree, BPlusTreeNode *node, BPlusTreeNode *right,
                    BPlusTreeNode *parent, int separationIdx)
{
    if (!tree || !node || !right || !parent)
        return;

    if (node->isLeaf)
    {
        // Copy all keys/data from right into node
        for (int i = 0; i < right->numKeys; i++)
        {
            node->keys[node->numKeys + i] = right->keys[i];
            node->data[node->numKeys + i] = right->data[i];
        }
        node->numKeys += right->numKeys;

        // CRITICAL: Update linked list to skip the deleted right node
        node->next = right->next;
    }
    else
    {
        // For internal nodes: bring down separator from parent
        node->keys[node->numKeys] = parent->keys[separationIdx];
        node->children[node->numKeys + 1] = right->children[0];
        node->numKeys++;

        // Copy all keys and children from right to node
        int childIdx = node->numKeys + 1;
        for (int i = 0; i < right->numKeys; i++)
        {
            node->keys[node->numKeys] = right->keys[i];
            node->children[childIdx++] = right->children[i + 1];
            node->numKeys++;
        }
    }

    // Remove separator from parent
    for (int i = separationIdx; i < parent->numKeys - 1; i++)
    {
        parent->keys[i] = parent->keys[i + 1];
        parent->children[i + 1] = parent->children[i + 2];
    }
    parent->numKeys--;

    // FREE THE MERGED RIGHT NODE
    freeNode(right);

    // Handle root case
    if (parent == tree->root && parent->numKeys == 0)
    {
        tree->root = node;
        freeNode(parent);
        return;
    }

    // Check if parent needs rebalancing
    int minKeys = parent->isLeaf ? (MAX_KEYS + 1) / 2 : (MAX_KEYS) / 2;
    if (parent->numKeys < minKeys)
    {
        rebalanceAfterDeletion(tree, parent);
    }
}

void rebalanceAfterDeletion(BPlusTree *tree, BPlusTreeNode *node)
{
    if (!tree || !node)
        return;

    // Root special case
    if (node == tree->root)
    {
        if (!node->isLeaf && node->numKeys == 0 && node->children[0])
        {
            BPlusTreeNode *newRoot = node->children[0];
            tree->root = newRoot;
            freeNode(node);
        }
        return;
    }

    // Calculate minimum keys for B+ tree nodes
    int minKeys;
    if (node->isLeaf)
    {
        // Leaf nodes need at least ceil((MAX_KEYS + 1) / 2) keys
        minKeys = (MAX_KEYS + 1) / 2;
    }
    else
    {
        // Internal nodes need at least ceil(MAX_KEYS / 2) keys
        minKeys = MAX_KEYS / 2;
    }

    // If node has enough keys, no rebalancing needed
    if (node->numKeys >= minKeys)
    {
        return;
    }

    // Find parent and node's position
    BPlusTreeNode *parent = findParent(tree->root, node);
    if (!parent)
    {
        return;
    }

    int nodeIndex = -1;
    for (int i = 0; i <= parent->numKeys; i++)
    {
        if (parent->children[i] == node)
        {
            nodeIndex = i;
            break;
        }
    }
    if (nodeIndex == -1)
    {
        return;
    }

    // Try borrowing from left sibling first
    if (nodeIndex > 0)
    {
        BPlusTreeNode *left = parent->children[nodeIndex - 1];

        if (left->isLeaf)
        {
            increment_leaf_nodes_access_count();
        }
        else
        {
            increment_index_nodes_access_count();
        }

        if (canBorrowFromLeft(left, node->isLeaf))
        {
            borrowFromLeft(node, left, parent, nodeIndex - 1);
            return;
        }
    }

    // Try borrowing from right sibling
    if (nodeIndex < parent->numKeys)
    {
        BPlusTreeNode *right = parent->children[nodeIndex + 1];

        if (right->isLeaf)
        {
            increment_leaf_nodes_access_count();
        }
        else
        {
            increment_index_nodes_access_count();
        }

        if (canBorrowFromRight(right, node->isLeaf))
        {
            borrowFromRight(node, right, parent, nodeIndex);
            return;
        }
    }

    // Cannot borrow, must merge

    // Prefer merging with left sibling
    if (nodeIndex > 0)
    {
        BPlusTreeNode *left = parent->children[nodeIndex - 1];

        // dont need increment here coz already incremented when
        // checking if can borrow

        mergeWithLeft(tree, node, left, parent, nodeIndex - 1);
        return;
    }
    else if (nodeIndex < parent->numKeys)
    {
        BPlusTreeNode *right = parent->children[nodeIndex + 1];

        // dont need increment here coz already incremented above

        mergeWithRight(tree, node, right, parent, nodeIndex);
        return;
    }
}

bool deleteKey(BufferManager *bm, BPlusTree *tree, float key)
{
    // printf("Calling delete key\n");
    if (!tree || !tree->root)
        return false;

    // Find the leaf containing the key
    BPlusTreeNode *leaf = findLeaf(tree->root, key);
    if (!leaf)
    {
        return false;
    }

    // Find the key in the leaf
    int keyIndex = -1;
    for (int i = 0; i < leaf->numKeys; i++)
    {
        if (fabs(leaf->keys[i] - key) < 1e-6)
        {
            // use fabs to avoid precision issues
            keyIndex = i;
            break;
        }
    }

    if (keyIndex == -1)
    {
        return false; // Key not found
    }

    // Mark the corresponding record as deleted before removing from tree
    markRecordAsDeleted(bm, leaf->data[keyIndex]);

    // Remove the key from the leaf
    // don't need to increment leaf nodes count here because
    // we considered it when finding leaf already --> don't double count
    removeFromLeaf(leaf, keyIndex);

    int minKeys = leaf->isLeaf ? (MAX_KEYS + 1) / 2 : (MAX_KEYS / 2);

    if ((leaf != tree->root && leaf->numKeys < minKeys) ||
        (leaf == tree->root && leaf->numKeys == 0 && !leaf->isLeaf))
    {
        // rebalance tree from leaf
        rebalanceAfterDeletion(tree, leaf);
    }

    return true;
}

void deleteKeysAboveThreshold(BufferManager *bm, BPlusTree *tree, float threshold)
{
    if (!tree || !tree->root)
        return;

    // RESET COUNTERS
    number_of_index_nodes_accessed = 0;
    number_of_leaf_nodes_accessed = 0;

    // Step 1: collect all keys > threshold (Traversal Timing)
    float *keysToDelete = NULL;
    int count = 0, capacity = 10;
    keysToDelete = malloc(sizeof(float) * capacity);

    // get the left most leaf node
    BPlusTreeNode *leaf = tree->root;
    while (!leaf->isLeaf)
    {
        increment_index_nodes_access_count();
        leaf = leaf->children[0];
    }
    // add the leaf node accessed in the while loop later

    startTimer(); // <-- Start traversal timer

    while (leaf)
    {
        increment_leaf_nodes_access_count(); // last leaf node accessed
        for (int i = 0; i < leaf->numKeys; i++)
        {
            if (leaf->keys[i] > threshold)
            {
                if (count >= capacity)
                {
                    capacity *= 2;
                    keysToDelete = realloc(keysToDelete, sizeof(float) * capacity);
                }
                keysToDelete[count++] = leaf->keys[i];
            }
        }
        leaf = leaf->next;
    }

    retrieval_time_bp = getElapsedTimeMillis(); // <-- Traversal done

    // Step 2: delete them one by one (Deletion Timing)
    startTimer(); // <-- Start deletion timer

    for (int i = 0; i < count; i++)
    {
        number_of_games_deleted++;
        deleteKey(bm, tree, keysToDelete[i]);
    }

    running_time_bp = getElapsedTimeMillis(); // <-- Deletion done
    free(keysToDelete);
}

void verifyTreeStructure(BPlusTree *tree)
{
    if (!tree || !tree->root)
    {
        printf("Tree is empty\n");
        return;
    }

    printf("Tree verification:\n");
    printf("- Leaf nodes: %d\n", countLeafNodes(tree));
    printf("- Total nodes: %d\n", countNodes(tree->root));
    printf("- Tree levels: %d\n", countLevels(tree));

    // Check if any leaf nodes have 0 keys (which might indicate deletion issues)
    BPlusTreeNode *leaf = tree->root;
    while (!leaf->isLeaf)
    {
        leaf = leaf->children[0];
    }

    int emptyLeaves = 0;
    while (leaf)
    {
        if (leaf->numKeys == 0)
        {
            emptyLeaves++;
        }
        leaf = leaf->next;
    }

    if (emptyLeaves > 0)
    {
        printf("WARNING: Found %d empty leaf nodes\n", emptyLeaves);
    }
}

ValidationResult checkNode(BPlusTreeNode *node, int depth, int order, bool isRoot)
{
    ValidationResult res = {true, -1};

    if (!node)
    {
        res.valid = false;
        return res;
    }

    int minChildren = (int)ceil(order / 2.0);
    int minKeysLeaf = (int)ceil((order - 1) / 2.0);

    // ===== Root special case =====
    if (isRoot)
    {
        if (!node->isLeaf && (node->numKeys < 1 || node->numKeys >= order))
        {
            res.valid = false;
            return res;
        }
        if (node->isLeaf && node->numKeys < 1)
        {
            res.valid = false;
            return res;
        }
    }
    else
    {
        // ===== Non-root cases =====
        if (node->isLeaf)
        {
            if (node->numKeys < minKeysLeaf || node->numKeys > order - 1)
            {
                res.valid = false;
                return res;
            }
        }
        else
        {
            int childrenCount = node->numKeys + 1;
            if (childrenCount < minChildren || childrenCount > order)
            {
                res.valid = false;
                return res;
            }
        }
    }

    // ===== Leaf handling =====
    if (node->isLeaf)
    {
        res.leafDepth = depth;
        return res;
    }

    // ===== Recurse into children =====
    for (int i = 0; i <= node->numKeys; i++)
    {
        ValidationResult childRes = checkNode(node->children[i], depth + 1, order, false);

        if (!childRes.valid)
        {
            res.valid = false;
            return res;
        }

        if (res.leafDepth == -1)
            res.leafDepth = childRes.leafDepth;
        else if (res.leafDepth != childRes.leafDepth)
        {
            // Leaves not at same depth
            res.valid = false;
            return res;
        }
    }

    return res;
}

bool validateBPlusTree(BPlusTree *tree)
{
    if (!tree || !tree->root)
        return true; // empty tree is valid
    ValidationResult res = checkNode(tree->root, 0, MAX_KEYS, true);

    if (res.valid)
    {
        printf("B+ tree structure is valid.\n");
    }
    else
    {
        printf("ERROR: B+ tree structure is invalid!\n");
    }

    return res.valid;
}

// ============= LAZY DELETION IMPLEMENTATION =============

/**
 * @brief Pure traversal phase: Collects keys and record pointers above threshold without any disk operations.
 *
 * This function provides a fair comparison with regular deletion's traversal phase
 * by performing only tree traversal and key collection in memory.
 *
 * @param tree The B+ tree.
 * @param threshold The deletion threshold.
 * @param keysToDelete Output array to store collected keys.
 * @param recordsToDelete Output array to store corresponding record pointers.
 * @param capacity Pointer to capacity of the arrays (will be updated if reallocated).
 * @return The number of keys collected.
 */
int collectKeysForLazyDeletion(BPlusTree *tree, float threshold, float **keysToDelete, NBA_Record_Pointer **recordsToDelete, int *capacity)
{
    if (!tree || !tree->root)
        return 0;

    int count = 0;

    // Initialize arrays if not provided
    if (!*keysToDelete)
    {
        *capacity = 10;
        *keysToDelete = malloc(sizeof(float) * (*capacity));
        *recordsToDelete = malloc(sizeof(NBA_Record_Pointer) * (*capacity));
    }

    // Find the leftmost leaf to begin traversal
    BPlusTreeNode *leaf = tree->root;
    while (!leaf->isLeaf)
    {
        increment_index_nodes_access_count(); // Count internal nodes
        leaf = leaf->children[0];
    }

    // Traverse all leaf nodes using the 'next' pointer (pure traversal, no disk I/O)
    while (leaf)
    {
        increment_leaf_nodes_access_count(); // Count each leaf accessed
        for (int i = 0; i < leaf->numKeys; i++)
        {
            if (leaf->keys[i] > threshold)
            {
                // Collect both keys and record pointers in memory (no disk operations)
                if (count >= *capacity)
                {
                    *capacity *= 2;
                    *keysToDelete = realloc(*keysToDelete, sizeof(float) * (*capacity));
                    *recordsToDelete = realloc(*recordsToDelete, sizeof(NBA_Record_Pointer) * (*capacity));
                }
                (*keysToDelete)[count] = leaf->keys[i];
                (*recordsToDelete)[count] = leaf->data[i];
                count++;
            }
        }
        leaf = leaf->next;
    }
    return count;
}

/**
 * @brief Phase 2 of lazy deletion: Marks collected records on disk as deleted.
 *
 * Takes the record pointers collected during pure traversal and marks them
 * as deleted on disk. This is much more efficient than searching for keys again.
 *
 * @param bm The buffer manager.
 * @param recordsToDelete Array of record pointers to mark as deleted.
 * @param count Number of records in the array.
 * @return The number of records marked for deletion.
 */
int markCollectedKeysAsDeleted(BufferManager *bm, NBA_Record_Pointer *recordsToDelete, int count)
{
    if (!recordsToDelete || count == 0)
        return 0;

    int markedCount = 0;

    // Mark each collected record as deleted directly using its pointer
    for (int k = 0; k < count; k++)
    {
        if (markRecordAsDeleted(bm, recordsToDelete[k]))
        {
            markedCount++;
        }
    }
    return markedCount;
}

/**
 * @brief Performs strict rebalancing after compaction to ensure all occupancy requirements
 */
void strictRebalance(BPlusTree *tree)
{
    if (!tree || !tree->root)
        return;

    int rebalance_rounds = 0;
    int max_rounds = 10; // Prevent infinite loops

    // Keep rebalancing until no more violations exist
    while (rebalance_rounds < max_rounds)
    {
        bool foundViolation = false;
        rebalance_rounds++;

        // Check and fix underfull LEAF nodes
        int leafMinKeys = getMinKeys(NULL, true); // 170 for leaf nodes
        bool leafViolations = fixUnderfullNodes(tree, leafMinKeys);

        // Check and fix underfull INTERNAL nodes
        int internalMinKeys = getMinKeys(NULL, false); // 169 for internal nodes
        bool internalViolations = fixUnderfullInternalNodes(tree, internalMinKeys);

        foundViolation = leafViolations || internalViolations;
    }
    // Final cleanup: check for root with single child
    if (tree->root && !tree->root->isLeaf && tree->root->numKeys == 0 && tree->root->children[0])
    {
        BPlusTreeNode *oldRoot = tree->root;
        tree->root = tree->root->children[0];
        freeNode(oldRoot);
    }
}

/**
 * @brief Fix all underfull nodes in the tree (returns true if any violations were found)
 */
bool fixUnderfullNodes(BPlusTree *tree, int minKeys)
{
    bool foundViolation = false;
    int violations_found = 0;

    // Find the leftmost leaf
    BPlusTreeNode *leaf = tree->root;
    while (leaf && !leaf->isLeaf)
    {
        increment_index_nodes_access_count();
        leaf = leaf->children[0];
    }

    // Process all leaf nodes
    BPlusTreeNode *current = leaf;
    while (current)
    {
        increment_leaf_nodes_access_count();
        BPlusTreeNode *next = current->next; // Save next before potential modification

        // Check if this node violates occupancy (but skip root)
        if (current != tree->root && current->numKeys < minKeys)
        {
            violations_found++;
            // printf("  LEAF violation #%d: node with %d keys (need %d)\n",
            //        violations_found, current->numKeys, minKeys);

            if (current->numKeys == 0)
            {
                // Handle completely empty nodes
                removeEmptyNode(tree, current);
            }
            else
            {
                // Try to rebalance with siblings
                rebalanceUnderfullNode(tree, current, minKeys);
            }

            foundViolation = true;
        }

        current = next;
    }

    // printf("  Total LEAF violations this round: %d\n", violations_found);
    return foundViolation;
}

/**
 * @brief Fix all underfull internal nodes in the tree
 */
bool fixUnderfullInternalNodes(BPlusTree *tree, int minKeys)
{
    bool foundViolation = false;
    int violations_found = 0;

    // Process internal nodes level by level (bottom-up)
    int height = countLevels(tree);

    increment_index_nodes_access_count(); // i think safe to increase only once coz got only one root node

    for (int level = height - 2; level >= 1; level--)
    { // Skip leaf level and root
        foundViolation |= fixInternalNodesAtLevel(tree, tree->root, 0, level, minKeys, &violations_found);
    }

    // printf("  Total INTERNAL violations this round: %d\n", violations_found);
    return foundViolation;
}

/**
 * @brief Helper function to fix internal nodes at a specific level
 */
bool fixInternalNodesAtLevel(BPlusTree *tree, BPlusTreeNode *node, int currentLevel, int targetLevel, int minKeys, int *violationCount)
{
    if (!node)
        return false;

    // Count every internal node visited during traversal
    if (!node->isLeaf && currentLevel > 0)
    {
        increment_index_nodes_access_count();
    }

    bool foundViolation = false;

    if (currentLevel == targetLevel)
    {
        // We're at the target level, check this internal node
        if (!node->isLeaf && node != tree->root && node->numKeys < minKeys)
        {
            (*violationCount)++;
            printf("  INTERNAL violation #%d: node at level %d with %d keys (need %d)\n",
                   *violationCount, currentLevel, node->numKeys, minKeys);

            // Try to fix the internal node (simplified - could be enhanced)
            rebalanceUnderfullNode(tree, node, minKeys);
            foundViolation = true;
        }
    }
    else if (currentLevel < targetLevel && !node->isLeaf)
    {
        // Continue traversing down to target level
        for (int i = 0; i <= node->numKeys; i++)
        {
            foundViolation |= fixInternalNodesAtLevel(tree, node->children[i], currentLevel + 1, targetLevel, minKeys, violationCount);
        }
    }

    return foundViolation;
}

/**
 * @brief Get correct minimum keys for a node based on B+ tree occupancy rules
 * @param node The node to check (or NULL to specify via isLeaf parameter)
 * @param isLeaf Whether this is a leaf node (used when node is NULL)
 * @return Minimum number of keys required
 */
int getMinKeys(BPlusTreeNode *node, bool isLeaf)
{
    bool leafNode = (node != NULL) ? node->isLeaf : isLeaf;

    if (leafNode)
    {
        // Leaf nodes: ⌈(n+1)/2⌉ where n = MAX_KEYS
        return (MAX_KEYS + 2) / 2; // = 170 for MAX_KEYS=338
    }
    else
    {
        // Internal nodes: ⌈n/2⌉ where n = MAX_KEYS
        return (MAX_KEYS + 1) / 2; // = 169 for MAX_KEYS=338
    }
}

/**
 * @brief Remove a completely empty node from the tree
 */
void removeEmptyNode(BPlusTree *tree, BPlusTreeNode *emptyNode)
{
    if (!emptyNode || emptyNode->numKeys > 0)
        return;

    // printf("    Removing empty node\n");

    // Find parent and remove reference to this node
    BPlusTreeNode *parent = findParent(tree->root, emptyNode);
    if (parent)
    {
        // Find the child pointer index
        int childIndex = -1;
        for (int i = 0; i <= parent->numKeys; i++)
        {
            if (parent->children[i] == emptyNode)
            {
                childIndex = i;
                break;
            }
        }

        if (childIndex != -1)
        {
            // Remove the child pointer and corresponding key
            for (int i = childIndex; i < parent->numKeys; i++)
            {
                parent->children[i] = parent->children[i + 1];
                if (i < parent->numKeys - 1)
                {
                    parent->keys[i] = parent->keys[i + 1];
                }
            }
            parent->numKeys--;
        }
    }

    // Update leaf linked list
    if (emptyNode->isLeaf)
    {
        // Find the leftmost leaf node
        BPlusTreeNode *prev = tree->root;
        while (prev && !prev->isLeaf)
        {
            increment_index_nodes_access_count();
            prev = prev->children[0];
        }

        // find the previous leaf node
        while (prev && prev->next != emptyNode)
        {
            increment_leaf_nodes_access_count();
            prev = prev->next;
        }

        if (prev)
        {
            prev->next = emptyNode->next;
        }
    }

    freeNode(emptyNode);
}

/**
 * @brief Rebalance an underfull node by borrowing or merging with siblings
 */
void rebalanceUnderfullNode(BPlusTree *tree, BPlusTreeNode *underfullNode, int minKeys)
{
    // index count counted in findParent method
    BPlusTreeNode *parent = findParent(tree->root, underfullNode);
    if (!parent)
        return; // Root node, skip

    // Find the position of this node in parent
    int nodeIndex = -1;
    for (int i = 0; i <= parent->numKeys; i++)
    {
        if (parent->children[i] == underfullNode)
        {
            nodeIndex = i;
            break;
        }
    }

    if (nodeIndex == -1)
        return;

    // Try to borrow from left sibling first
    if (nodeIndex > 0)
    {
        BPlusTreeNode *leftSibling = parent->children[nodeIndex - 1];

        if (leftSibling->isLeaf)
        {
            increment_leaf_nodes_access_count();
        }
        else
        {
            increment_index_nodes_access_count();
        }

        if (leftSibling->numKeys > minKeys)
        {
            borrowFromLeft(underfullNode, leftSibling, parent, nodeIndex - 1);
            return;
        }
    }

    // Try to borrow from right sibling
    if (nodeIndex < parent->numKeys)
    {
        BPlusTreeNode *rightSibling = parent->children[nodeIndex + 1];

        if (rightSibling->isLeaf)
        {
            increment_leaf_nodes_access_count();
        }
        else
        {
            increment_index_nodes_access_count();
        }

        if (rightSibling->numKeys > minKeys)
        {
            borrowFromRight(underfullNode, rightSibling, parent, nodeIndex);
            return;
        }
    }

    // Cannot borrow, must merge
    if (nodeIndex > 0)
    {
        // Merge with left sibling
        BPlusTreeNode *leftSibling = parent->children[nodeIndex - 1];
        mergeWithLeft(tree, underfullNode, leftSibling, parent, nodeIndex - 1);
    }
    else if (nodeIndex < parent->numKeys)
    {
        // Merge with right sibling
        BPlusTreeNode *rightSibling = parent->children[nodeIndex + 1];
        mergeWithRight(tree, underfullNode, rightSibling, parent, nodeIndex);
    }
}

/**
 * @brief Compacts leaf nodes by removing entries pointing to deleted records
 */
void compactDeletedEntries(BPlusTree *tree, float threshold)
{
    if (!tree || !tree->root)
        return;

    // Find the leftmost leaf
    BPlusTreeNode *leaf = tree->root;
    while (leaf && !leaf->isLeaf)
    {
        increment_index_nodes_access_count();
        leaf = leaf->children[0];
    }

    int total_removed = 0;
    int nodes_compacted = 0;

    // Traverse all leaf nodes and compact them
    while (leaf)
    {
        increment_leaf_nodes_access_count();
        int originalKeys = leaf->numKeys;
        int writeIdx = 0;

        // Compact by removing keys above threshold
        for (int readIdx = 0; readIdx < leaf->numKeys; readIdx++)
        {
            if (leaf->keys[readIdx] <= threshold)
            { // Keep only non-deleted keys
                if (writeIdx != readIdx)
                {
                    leaf->keys[writeIdx] = leaf->keys[readIdx];
                    leaf->data[writeIdx] = leaf->data[readIdx];
                }
                writeIdx++;
            }
        }

        leaf->numKeys = writeIdx;
        int removed = originalKeys - writeIdx;
        if (removed > 0)
        {
            total_removed += removed;
            nodes_compacted++;
        }

        leaf = leaf->next;
    }
    // printf("Phase 2 Compaction complete: removed %d entries from %d nodes\n\n", total_removed, nodes_compacted);
}

/**
 * @brief Performs a three-phase lazy deletion of keys above a given threshold.
 *
 * Phase 1: Pure traversal - Only collects keys above threshold (for fair comparison)
 * Phase 2: Disk marking - Marks collected records as deleted on disk
 * Phase 3: Tree compaction - Compacts the B+ tree and rebalances structure
 *
 * @param bm The buffer manager to handle disk I/O.
 * @param tree The B+ tree to operate on.
 * @param threshold The value above which keys will be deleted.
 */
void lazyDeleteKeysAboveThreshold(BufferManager *bm, BPlusTree *tree, float threshold)
{
    if (!tree || !tree->root)
        return;

    // RESET COUNTERS
    number_of_index_nodes_accessed = 0;
    number_of_leaf_nodes_accessed = 0;

    float *keysToDelete = NULL;
    NBA_Record_Pointer *recordsToDelete = NULL;
    int capacity = 0;

    // Phase 1: Pure traversal (comparable to regular deletion's traversal)
    // printf("Phase 1: Pure traversal and key collection...\n");
    startTimer();

    int keyCount = collectKeysForLazyDeletion(tree, threshold, &keysToDelete, &recordsToDelete, &capacity);

    lazy_pure_traversal_time = getElapsedTimeMillis();
    // Phase 2: Disk marking operations
    // printf("Phase 2: Marking records on disk...\n");
    startTimer();

    int markedCount = markCollectedKeysAsDeleted(bm, recordsToDelete, keyCount);
    number_of_games_deleted = markedCount; // Update statistics

    lazy_disk_marking_time = getElapsedTimeMillis();

    // Phase 3: Tree compaction and rebalancing
    // printf("Phase 3: Tree compaction and rebalancing...\n");
    startTimer();

    compactDeletedEntries(tree, threshold);
    strictRebalance(tree);

    lazy_compaction_time = getElapsedTimeMillis();
    int phase3_internal = number_of_index_nodes_accessed;
    int phase3_leaf = number_of_leaf_nodes_accessed;

    // Set timing variables for compatibility with existing reporting
    retrieval_time_bp = lazy_pure_traversal_time;                    // Pure traversal for fair comparison
    running_time_bp = lazy_disk_marking_time + lazy_compaction_time; // Actual work phases

    // Cleanup
    free(keysToDelete);
    free(recordsToDelete);
}