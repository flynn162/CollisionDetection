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
        if (this->size > BUFFER_SIZE - MAX_WEIGHT) {
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

void BaseBPTree::destroy_iteration_buffer(Acc* acc) {
    delete acc;
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
    size_t i = 0;
    while (curr != nullptr && curr->keys[0] <= k1) {
        // go through a leaf node and extract keys in the range [k0, k1]
        while (curr->keys[i] <= k1) {
            if (curr->keys[i] >= k0)
                out->put(curr->values[i + 1].p);
            i++;
        }
        // go to the next sibling leaf node
        // (because we may have stopped at an INFINITY mark)
        curr = curr->next;
        i = 0;
        out->ensure_space();
    }
    out->flush();
}

void BaseBPTree::test_if_values_are_sorted(float since) {
    auto curr = find_leaf(since, this->root);
    float last_key = -INFINITY;
    while (curr != nullptr) {
        if (last_key > curr->keys[0]) {
            throw std::logic_error("not sorted!");
        }
        for (size_t i = 0; i < MAX_WEIGHT - 1; i++) {
            if (curr->keys[i] > curr->keys[i + 1]) {
                throw std::logic_error("not sorted within a node");
            }
            if (isinf(curr->keys[i + 1])) {
                last_key = curr->keys[i];
                break;
            }
        }
        curr = curr->next;
    }
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

template<class T>
static void swap_values(BPTreeNode* self, size_t i, T** vo) {
    throw std::logic_error("not implemented");
}
template<>
void swap_values<void>(BPTreeNode* self, size_t i, void** vo) {
    // leaf node
    void* temp = self->values[i + 1].p;
    self->values[i + 1].p = *vo;
    *vo = temp;
}
template<>
void swap_values<BPTreeNode>(BPTreeNode* self, size_t i, BPTreeNode** vo) {
    // internal node
    BPTreeNode* temp = self->values[i + 1].b;
    self->values[i + 1].b = *vo;
    *vo = temp;
}

template<class T>
static bool insert_into(BPTreeNode* self, float key, T** value_out) {
    // Insert key and value into a non-full node, and put the old value into
    // `value_out`. If the key is new, put a nullptr into `value_out`.
    //
    // Behavior is undefined if the node is full.

#ifdef DEBUG
    if (!isinf(self->keys[MAX_WEIGHT - 1]))
        throw std::logic_error("node is full");
#endif

    // find the first empty slot in the node
    size_t i = 0;
    while (self->keys[i] != key && !isinf(self->keys[i]))
        i++;

    // insert key and value into the empty slot
    // or if they key already exists, it's original slot
    self->keys[i] = key;
    swap_values<T>(self, i, value_out);

    // sort
    insertion_sort(self, i);
    // return "is node full"
    return i == MAX_WEIGHT - 1;
}

constexpr size_t index_to_split_at() {
    return MAX_WEIGHT / 2;
}

static BPTreeNode* split_node(BPTreeNode* self, float* key_out) {
    // Split a full node into two. Return the new node that is allocated.
    // The "lifted key" is written to `key_out`

    constexpr size_t i = index_to_split_at();
    size_t bytes_f = (MAX_WEIGHT - i - 1) * sizeof(self->keys[0]);
    size_t bytes_p = (MAX_WEIGHT - i) * sizeof(self->values[0]);

    *key_out = self->keys[i];  // the key to be lifted
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

static BPTreeNode* insert(float* key_out, void** value_out, BPTreeNode* curr) {
    // Private recursive method for inserting a key into the tree
    //
    // Returns a new BPTreeNode if there was a need to create one. If no new
    // node was created, we return nullptr.
    //
    // Out parameters: If we created a new node, the "lifted key" will be
    // written to `key_out`; otherwise, we leave `key_out` unchanged. If the
    // inserting operation replaces an existing value under the same key, the
    // "old value" will be written to `value_out`; otherwise, a nullptr will be
    // written to `value_out`.

    if (curr != curr->next) {
        // base case: leaf node
        if (insert_into<void>(curr, *key_out, value_out))
            return split_node(curr, key_out);  // node becomes full
        else
            return nullptr;
    } else {
        // internal node case
        float kxchg = *key_out;
        // find the appropriate index
        size_t i = 0;
        while (curr->keys[i] <= kxchg) i++;
        // descend into a child node
#ifdef DEBUG
        if (curr->values[i].b == nullptr)
            throw std::logic_error("corrupted internal node");
#endif
        BPTreeNode* new_node = insert(&kxchg, value_out, curr->values[i].b);
        if (new_node != nullptr) {
            // a new node was created; the lifted key was written into kxchg
            // we should insert kxchg into the current node
            if (insert_into<BPTreeNode>(curr, kxchg, &new_node))
                return split_node(curr, key_out);
        }
        return nullptr;
    }
}

void* BaseBPTree::replace_p(float key, void* value) {
    BPTreeNode* new_node = insert(&key, &value, this->root);
    if (new_node != nullptr) {
        // root node was full and was split into two
        // a new node was allocated; lifted key was written to `key`
        // make a new root
        // add the lifted key to the new root
        BPTreeNode* new_root = make_bptree_node();
        new_root->keys[0] = key;
        new_root->values[0].b = this->root;
        new_root->values[1].b = new_node;
        new_root->next = new_root;  // mark as internal node
        this->root = new_root;
    }
    return value;
}

void BaseBPTree::update_p(float old_key, float new_key, void* value) {
    // not implemented
}

void BaseBPTree::delete_p(float key, void* match_value) {
    // not implemented
}
