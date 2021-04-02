#pragma once
#include <string.h>
#include <stdlib.h>
#include <stdalign.h>
#include <math.h>
#include "Config.h"

#define SET_NODE_DATA_SIZE 6
#define SET_HEADER_DATA_SIZE 5

typedef struct SetNode SetNode;
struct SetNode {
    void* data[SET_NODE_DATA_SIZE];
    SetNode* next;
    SetNode* prev;
};

typedef struct {
    float nan_box;
    SetNode* first;
    SetNode* last;
    void* data[SET_HEADER_DATA_SIZE];
} Collection;

typedef union {
    float nan_box;
    unsigned char raw[4];
} RawFloatData;

static SetNode* SetNode_make(SetNode* prev, void* element, SetNode* next) {
    SetNode* result = (SetNode*) aligned_alloc(ALIGN_CACHE, sizeof(SetNode));
    result->data[0] = element;
    result->next = next;
    result->prev = prev;
    return result;
}

static void SetNode_destroy(SetNode* this) {
    free(this);
}

static size_t Collection_last_node_size(Collection* this) {
    // Get the last node's size from NaN box
    // Read from the Common Initial Sequence. If not NaN, return 0
    if (!isnan(this->nan_box)) {
        return 0;  // sanity check
    }
    RawFloatData data;
    data.nan_box = this->nan_box;
    return data.raw[0];
}

static void Collection_set_last_node_size(Collection* this, size_t value) {
    RawFloatData data;
    data.nan_box = nanf("");
    data.raw[0] = (unsigned char) value;
    this->nan_box = data.nan_box;
}

static Collection* Collection_make(void* initial_element) {
    Collection* result =
        (Collection*) aligned_alloc(ALIGN_CACHE, sizeof(Collection));
    Collection_set_last_node_size(result, 1);
    result->first = NULL;
    result->last = NULL;
    result->data[0] = initial_element;
    return result;
}

static void Collection_destroy(Collection* this) {
    SetNode* to_be_freed = this->first;
    free(this);
    while (to_be_freed != NULL) {
        SetNode* temp = to_be_freed->next;
        SetNode_destroy(to_be_freed);
        to_be_freed = temp;
    }
}

static void Collection_add(Collection* self, void* value) {
    size_t last_size = Collection_last_node_size(self);
    if (self->last == NULL) {
        // The collection does not have a "tail"
        if (last_size < SET_HEADER_DATA_SIZE) {
            self->data[last_size] = value;
            Collection_set_last_node_size(self, last_size + 1);
        } else {
            SetNode* new_node = SetNode_make(NULL, value, NULL);
            Collection_set_last_node_size(self, 1);
            self->first = new_node;
            self->last = new_node;
        }
    } else {
        // The collection has a tail
        if (last_size < SET_NODE_DATA_SIZE) {
            self->last->data[last_size] = value;
            Collection_set_last_node_size(self, last_size + 1);
        } else {
            SetNode* new_node = SetNode_make(self->last, value, NULL);
            Collection_set_last_node_size(self, 1);
            self->last->next = new_node;
            self->last = new_node;
        }
    }
}
