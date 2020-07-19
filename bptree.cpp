#include <stdint.h>
#include <math.h>
#include <string.h>
#include <limits>
#include <stdexcept>
#include "bptree.hpp"

constexpr size_t MAX_WEIGHT = 20;
constexpr size_t BUFFER_SIZE = 80;


struct alignas(64) BaseBPTree::Node {
    union {
        BaseBPTree::Node* next;
        size_t index_of_next;
    };
    float keys[MAX_WEIGHT];
    union {
        void* p;
        BaseBPTree::Node* b;
        size_t i;
    } values[MAX_WEIGHT + 1];
};

using BPTreeNode = BaseBPTree::Node;
static_assert(struct_size_is_appropriate<BPTreeNode>());
static_assert(std::numeric_limits<float>::is_iec559, "need IEEE 754");

static BPTreeNode* make_bptree_node() {
    BPTreeNode* result = new BPTreeNode();
    for (size_t i = 0; i < MAX_WEIGHT; i++) {
        result->keys[i] = INFINITY;
        result->values[i].p = nullptr;
    }
    result->values[MAX_WEIGHT].p = nullptr;
    result->next = nullptr;
    return result;
}


class BaseBPTree::Acc {
public:
    Acc(BaseBPTree* parent) {
        this->parent = parent;
        this->size = 0;
    }
    ~Acc() = default;

    void put(void* item) {
        this->buffer[this->size] = item;
        this->size++;
    }

    void ensure_space() {
        if (BUFFER_SIZE - this->size < MAX_WEIGHT) {
            this->parent->callback(this->buffer, this->size);
            this->size = 0;
        }
    }

    void flush() {
        if (this->size > 0) {
            this->parent->callback(this->buffer, this->size);
            this->size = 0;
        }
    }

private:
    BaseBPTree* parent;
    size_t size;
    void* buffer[BUFFER_SIZE];
};


BaseBPTree::BaseBPTree() {
    this->root = make_bptree_node();
}

BaseBPTree::~BaseBPTree() {
    // not implemented
}

BaseBPTree::Acc* BaseBPTree::make_iteration_buffer() {
    return new BaseBPTree::Acc(this);
}

static inline BPTreeNode* find_leaf(float key, BPTreeNode* curr) {
    // Tail recursive helper method for finding the leaf node
    // The returned leaf node has keys greater than or equal to `key`

    while (curr != nullptr && curr->next == curr) {
        size_t i = 0;
        while (curr->keys[i] <= key)
            i++;
        curr = curr->values[i].b;
    }
    return curr;
}

void BaseBPTree::search_p(float key, BaseBPTree::Acc* out) {
    this->range_search_p(key, key, out);
}

void BaseBPTree::range_search_p(float k0, float k1, BaseBPTree::Acc* out) {
    auto curr = find_leaf(k0, this->root);
    while (curr != nullptr && curr->keys[0] <= k1) {
        // go through a leaf node and extract keys in the range [k0, k1]
        size_t i = 0;
        while (curr->keys[i] <= k1) {
            if (curr->keys[i] >= k0)
                out->put(curr->values[i + 1].p);
            i++;
        }
        // go to the next sibling leaf node
        // (because we may have stopped at an INFINITY mark)
        curr = curr->next;
        out->ensure_space();
    }
    out->flush();
}

static void insertion_sort(BPTreeNode* self, size_t idx) {
    float original = self->keys[idx];
    while (idx > 0 && self->keys[idx - 1] > original) {
        // swap keys[idx - 1] and keys[idx]
        self->keys[idx] = self->keys[idx - 1];
        self->keys[idx - 1] = original;

        // swap values[idx] and values[idx + 1]
        if (self->next != self) {  // leaf node
            void* temp = self->values[idx + 1].p;
            self->values[idx + 1].p = self->values[idx].p;
            self->values[idx].p = temp;
        } else {
            BPTreeNode* temp = self->values[idx + 1].b;
            self->values[idx + 1].b = self->values[idx].b;
            self->values[idx].b = temp;
        }
        idx--;
    }
}

static bool insert_into(BPTreeNode* self, float key, void* value) {
    // Insert key and value into a non-full node

    if (!isinf(self->keys[MAX_WEIGHT - 1]))
        throw std::logic_error("node is full");

    // find the first empty slot in the node
    size_t i = 0;
    while (self->keys[i] != key && !isinf(self->keys[i]))
        i++;

    // insert key and value into the empty slot
    // or if they key already exists, it's original slot
    self->keys[i] = key;
    if (self->next != self) {
        // leaf node
        self->values[i + 1].p = value;
    } else {
        // internal node
        self->values[i + 1].b = static_cast<BPTreeNode*>(value);
    }

    // sort
    insertion_sort(self, i);
    // return "is node full"
    return i == MAX_WEIGHT - 1;
}

constexpr size_t index_to_split_at() {
    return MAX_WEIGHT / 2;
}

static BPTreeNode* split_node(BPTreeNode* self) {
    // Split a full node into two. Return the new node that is allocated

    constexpr size_t i = index_to_split_at();
    size_t bytes_f = (MAX_WEIGHT - i - 1) * sizeof(self->keys[0]);
    size_t bytes_p = (MAX_WEIGHT - i) * sizeof(self->values[0]);

    BPTreeNode* new_node = make_bptree_node();

    if (self->next == self) {
        // internal node
        memcpy(new_node->keys,   &(self->keys[i + 1]),   bytes_f);
        memcpy(new_node->values, &(self->values[i + 1]), bytes_p);
        new_node->next = new_node;
    } else {
        // leaf node
        new_node->keys[0] = self->keys[i];
        memcpy(&(new_node->keys[1]),   &(self->keys[i + 1]),   bytes_f);
        memcpy(&(new_node->values[1]), &(self->values[i + 1]), bytes_p);
        new_node->next = self->next;
        self->next = new_node;
    }

    // zero out portions of the original node
    for (size_t j = i; j < MAX_WEIGHT; j++) {
        self->keys[j] = INFINITY;
    }
    memset(&(self->values[i + 1]), 0, bytes_p);

    return new_node;
}

static BPTreeNode* insert(float key, void* value, BPTreeNode* curr) {
    if (curr == nullptr) {
        // base case: empty leaf node
        auto new_node = make_bptree_node();
        new_node->keys[0] = key;
        new_node->values[1].p = value;
        return new_node;
    } else if (curr != curr->next) {
        // base case: leaf node
        if (insert_into(curr, key, value))
            return split_node(curr);  // node becomes full
        else
            return nullptr;
    } else {
        // internal node
        float key_to_insert = curr->keys[index_to_split_at()];
        // find a place to descent into
        size_t idx = 0;
        while (curr->keys[idx] <= key)
            idx++;
        // descent and perform recursion
        BPTreeNode* new_node = insert(key, value, curr->values[idx].b);
        if (new_node != nullptr) {
            // insert the floating point key into the current node
            if (insert_into(curr, key_to_insert, new_node))
                return split_node(curr);
        }
        return nullptr;
    }
}

void BaseBPTree::insert_p(float key, void* value) {
    float key_to_insert = this->root->keys[index_to_split_at()];
    BPTreeNode* new_node = insert(key, value, this->root);
    if (new_node != nullptr) {
        // add a key to our root
        if (insert_into(this->root, key_to_insert, new_node)) {
            // root node full, split
            BPTreeNode* new_node = split_node(this->root);
            // make a new root node
            BPTreeNode* new_root = make_bptree_node();
            new_root->keys[0] = key_to_insert;
            new_root->values[0].b = this->root;
            new_root->values[1].b = new_node;
            this->root = new_root;
        }
    }
}

void BaseBPTree::update_p(float old_key, float new_key, void* value) {
    // not implemented
}

void BaseBPTree::delete_p(float key, void* match_value) {
    // not implemented
}
