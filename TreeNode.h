#pragma once
#include <stdalign.h>
#include <stdlib.h>
#include <math.h>

#define MAX_WEIGHT 20
#define MIN_WEIGHT (MAX_WEIGHT / 2 - 1)
#define BUFFER_SIZE 80

typedef struct TreeNode TreeNode;
struct TreeNode {
    union {
        TreeNode* next;
        size_t index_of_next;
    };
    float keys[MAX_WEIGHT];
    union {
        void* p;
        TreeNode* b;
        size_t i;
    } values[MAX_WEIGHT + 1];
};

static TreeNode* TreeNode_make() {
    TreeNode* result = (TreeNode*) aligned_alloc(64, sizeof(TreeNode));
    for (size_t i = 0; i < MAX_WEIGHT; i++) {
        result->keys[i] = INFINITY;
        result->values[i].p = NULL;
    }
    result->values[MAX_WEIGHT].p = NULL;
    result->next = NULL;
    return result;
}

static void TreeNode_destroy(TreeNode* this) {
    free(this);
}

static size_t TreeNode_weight(TreeNode* this) {
    size_t weight = 0;
    while (!isinf(this->keys[weight])) weight++;
    return weight;
}
