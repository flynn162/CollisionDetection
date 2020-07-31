#include <stdint.h>
#include <math.h>
#include <string.h>
#include <limits>
#include <stdexcept>
#include <algorithm>
#include "bptree.hpp"

constexpr size_t MAX_WEIGHT = 20;
constexpr size_t MIN_WEIGHT = MAX_WEIGHT / 2 - 1;
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

void BaseBPTree::test_if_root_is_non_degenerate() {
    auto ptr = this->root->next;
    // One of the following must be true:
    // 1. The root is an internal node.
    // 2. The root is a leaf node with no siblings.
    if (ptr != this->root && ptr != nullptr)
        throw std::logic_error("root node is broken (degenerate)");
}


void BaseBPTree::update_p(float old_key, float new_key) {
    // not implemented
}


static size_t get_node_weight(BPTreeNode* node) {
    size_t weight = 0;
    while (!isinf(node->keys[weight])) weight++;
    return weight;
}

template<class T>
static void copy_value(BPTreeNode* self, size_t idst, size_t isrc) {
    throw std::logic_error("not implemented");
}
template<>
void copy_value<void>(BPTreeNode* self, size_t idst, size_t isrc) {
    // leaf node
    self->values[idst].p = self->values[isrc].p;
}
template<>
void copy_value<BPTreeNode>(BPTreeNode* self, size_t idst, size_t isrc) {
    // internal node
    self->values[idst].b = self->values[isrc].b;
}

template<class T>
static void clear_value(BPTreeNode* self, size_t idst) {
    throw std::logic_error("not implemented");
}
template<>
void clear_value<void>(BPTreeNode* self, size_t idst) {
    // leaf node
    self->values[idst].p = nullptr;
}
template<>
void clear_value<BPTreeNode>(BPTreeNode* self, size_t idst) {
    // internal node
    self->values[idst].b = nullptr;
}

template<class T>
static size_t delete_key_from_node(BPTreeNode* curr, float key) {
    // compute the node's old weight and find the key
    int key_idx = -1;
    size_t weight = 0;
    while (!isinf(curr->keys[weight])) {
        key_idx = (curr->keys[weight] == key) ? weight : key_idx;
        weight++;
    }

    if (key_idx >= 0) {
        // key was found
        // move the keys
        for (size_t i = key_idx + 1; i < weight; i++) {
            curr->keys[i - 1] = curr->keys[i];
        }
        curr->keys[weight - 1] = INFINITY;
        // move the values
        for (size_t i = key_idx + 2; i < weight + 1; i++) {
            copy_value<T>(curr, i - 1, i);
        }
        clear_value<T>(curr, weight);

        return weight - 1;
    } else {
        // key was not found
        return weight;
    }
}

static size_t compute_nkb(size_t receiver_weight, size_t sender_weight) {
#ifdef DEBUG
    if (receiver_weight >= sender_weight)
        throw std::logic_error("weight");
#endif
    constexpr size_t target_weight = MAX_WEIGHT * 2 / 3;
    size_t sender_gives = (sender_weight > target_weight)
        ? (sender_weight - target_weight)
        : 1;
    size_t receiver_wants = (target_weight > receiver_weight)
        ? (target_weight - receiver_weight)
        : 1;
    return std::min(sender_gives, receiver_wants);
}

static float borrow_keys_R(BPTreeNode* recv, size_t recv_weight, float ikey) {
    // Move keys and values from the sender (the node to my right) to the
    // receiver so that both the receiver and the sender are appropriately
    // weighted.
    //
    // Behavior is undefined if `recv` is the last child of its parent.
    //
    // :param recv: pointer to receiver node
    // :param recv_weight: original weight of the receiver
    // :param ikey: intermediate key (equals parent[sender_key_index])
    // :return: the sender's new key

    BPTreeNode* send = recv->next;
    size_t send_weight = get_node_weight(send);

    // Compute the number of keys to borrow
    size_t nkb = compute_nkb(recv_weight, send_weight);

    //        v                  v      (nkb=4)
    //     +-+-+-+-+-+    +-+-+-+-+-+
    //     |A| | | | |    |X|Y|Z|W|U|
    // ... +-\-\-\-\-\-+  +-\-\-\-\-\-+ ...
    //       |a| | | | |  |p|x|y|z|w|u|
    //       +-+-+-+-+-+  +-+-+-+-+-+-+
    //          ^                  ^
    //
    //         Receiver    Sender

    // Check if we need to keep send->values[0]
    // Note: if intermediate_key == keys[0] then values[0] is null
    if (ikey != send->keys[0]) {
        // this case is only true for internal nodes
        BPTreeNode* node_ptr = send->values[0].b;
        if (node_ptr != nullptr) {
            // do not drop the node_ptr
            recv->keys[recv_weight] = ikey;
            recv->values[recv_weight + 1].b = node_ptr;
            // after weight increment, `nkb` is still safe to use
            recv_weight++;
            send->values[0].b = nullptr;
            // this is because we'll return send->keys[0];
        }
    }

    //          v                  v      (nkb=4) (safe)
    //     +-+-+-+-+-+-+    +-+-+-+-+-+
    //     |A|I| | | | |    |X|Y|Z|W|U|
    // ... +-\-\-\-\-\-+-+  +-\-\-\-\-\-+ ...
    //       |a|p| | | | |  | |x|y|z|w|u|
    //       +-+-+-+-+-+-+  +-+-+-+-+-+-+
    //          * ^          0       ^
    //
    //           Receiver    Sender

    // copy keys and values
    size_t size_f = nkb * sizeof(float);
    size_t size_u = nkb * sizeof(recv->values[0]);
    memcpy(&(recv->keys[recv_weight]),   &(send->keys[0]),   size_f);
    memcpy(&(recv->values[recv_weight]), &(send->values[1]), size_u);

    //        v                  v      (nkb=4)
    //     +-+-+-+-+-+    +-+-+-+-+-+
    //     |A|X|Y|Z|W|    |X|Y|Z|W|U|
    // ... +-\-\-\-\-\-+  +-\-\-\-\-\-+ ...
    //       |a|x|y|z|w|  | |x|y|z|w|u|
    //       +-+-+-+-+-+  +-+-+-+-+-+-+
    //          ^          0       ^
    //
    //         Receiver    Sender

    // reorganize sender's key
    for (size_t i = nkb; i < send_weight; i++) {
        send->keys[i - nkb] = send->keys[i];
    }
    for (size_t i = send_weight - nkb; i < send_weight; i++) {
        send->keys[i] = INFINITY;
    }

    // reorganize sender's value pointers
    for (size_t i = nkb + 1; i < send_weight + 1; i++) {
        // send->values[i - nkb] = send->values[i]
        constexpr size_t sz = sizeof(send->values[0]);
        memcpy(&(send->values[i - nkb]), &(send->values[i]), sz);
    }
    // size_u = sizeof(send->values[0]) * nkb;  /* need not recompute this */
    memset(&(send->values[send_weight+1 - nkb]), 0, size_u);

    //        v                       v      (nkb=4)
    //     +-+-+-+-+-+         +-+-+-+-+-+
    //     |A|X|Y|Z|W|         |U|*|*|*|*|
    // ... +-\-\-\-\-\-+ ...   +-\-\-\-\-\-+ ...
    //       |a|x|y|z|w|       | |u|*|*|*|*|
    //       +-+-+-+-+-+       +-+-+-+-+-+-+
    //          ^                       ^
    //
    //         Receiver         Sender

    return send->keys[0];
}

static float borrow_keys_L(BPTreeNode* sndr, size_t sndr_weight, float ikey) {
    // Move keys and values from the sender to the receiver so that both the
    // sender and the receiver are appropriately weighted.
    //
    // Behavior is undefined if the receiver is a child of index 0 in its
    // parent node.
    //
    // :param sndr: the node to the left of the receiver.
    // :param sndr_weight: original sender weight
    // :param ikey: intermediate key which equals parent[receiver_key_index]
    // :return: the receiver's new key

    BPTreeNode* recv = sndr->next;
    size_t recv_weight = get_node_weight(recv);
    size_t nkb = compute_nkb(recv_weight, sndr_weight);

    //        v                         (nkb=4)
    //     +-+-+-+-+-+    +-+-+-+-+-+
    //     |A|B|C|D|E|    |X|Y|Z|W|.|
    // ... +-\-\-\-\-\-+  +-\-\-\-\-+-+ ...
    //       |a|b|c|d|e|  |p|x|y|z|w|.|
    //       +-+-+-+-+-+  +-+-+-+-+-+-+
    //          ^                  *
    //
    //           Sender    Receiver

    // move the receiver's keys and values to make space
    for (int i = recv_weight - 1; i >= 0; i--) {  // we must use "int" here
        recv->keys[i + nkb] = recv->keys[i];
    }
    // Note that we will move values[0] to values[nkb]
    for (int i = recv_weight; i >= 0; i--) {
        // recv->values[i + nkb] = recv->values[i];
        constexpr size_t sz = sizeof(recv->values[0]);
        memcpy(&(recv->values[i + nkb]), &(recv->values[i]), sz);
    }
    // zero out the first value pointer in `recv`
    // we do this because we will return keys[0]
    memset(recv->values, 0, sizeof(recv->values[0]));

    //        v                         (nkb=4)
    //     +-+-+-+-+-+    +-+-+-+-+-+
    //     |A|B|C|D|E|    |.|.|.|.|X|
    // ... +-\-\-\-\-\-+  +-\-\-\-\-\-+ ...
    //       |a|b|c|d|e|  | |.|.|.|p|x|
    //       +-+-+-+-+-+  +-+-+-+-+-+-+
    //          ^          0       *
    //
    //           Sender    Receiver

    // Check if we need to pull down the intermediate key
    if (ikey != recv->keys[nkb] && recv->values[nkb].b != nullptr) {
        // put the intermediate key into the receiver
        recv->keys[nkb - 1] = ikey;
        // since we used a slot, decrease nkb by one
        nkb--;
    }

    if (nkb == 0) return ikey;

    //          v                       (nkb=3)
    //     +-+-+-+-+-+    +-+-+-+-+-+
    //     |A|B|C|D|E|    |.|.|.|I|X|
    // ... +-\-\-\-\-\-+  +-\-\-\-\-\-+ ...
    //       |a|b|c|d|e|  | |.|.|.|p|x|
    //       +-+-+-+-+-+  +-+-+-+-+-+-+
    //            ^                *
    //
    //           Sender    Receiver

    // move keys
    // move values (from sender to recv->values[1] etc)
    size_t size_f;
    size_f = sizeof(float) * nkb;
    size_u = sizeof(recv->values[0]) * nkb;
    memcpy(&(recv->keys[0]),   &(sndr->keys[sndr_weight - nkb]),     size_f);
    memcpy(&(recv->values[1]), &(sndr->values[sndr_weight+1 - nkb]), size_u);

    //        v                         (nkb=4)
    //     +-+-+-+-+-+    +-+-+-+-+-+
    //     |A|B|C|D|E|    |B|C|D|E|X|
    // ... +-\-\-\-\-\-+  +-\-\-\-\-\-+ ...
    //       |a|b|c|d|e|  | |b|c|d|e|x|
    //       +-+-+-+-+-+  +-+-+-+-+-+-+
    //          ^                  *
    //
    //           Sender    Receiver

    // clean up
    for (size_t i = sndr_weight - nkb; i < sndr_weight; i++) {
        sndr->keys[i] = INFINITY;
    }
    memset(&(sndr->values[sndr_weight+1 - nkb]), 0, size_u);

    //        v                         (nkb=4)
    //     +-+-+-+-+-+    +-+-+-+-+-+
    //     |A| | | | |    |B|C|D|E|X|
    // ... +-\-\-\-\-\-+  +-\-\-\-\-\-+ ...
    //       |a| | | | |  | |b|c|d|e|x|
    //       +-+-+-+-+-+  +-+-+-+-+-+-+
    //          ^                  *
    //
    //           Sender    Receiver

    return recv->keys[0];
}

template<class T>
static void delete_key(BPTreeNode* parent, size_t idxk, float* ogkey) {
    auto curr = parent->values[idxk + 1].b;
    size_t new_weight = delete_key_from_node<T>(curr, ogkey);
    if (new_weight < MIN_WEIGHT) {
    } else {
        // adjust key (e.g. what if we deleted the smallest key?)
        parent->keys[idxk] = curr->keys[0];
    }
}

void BaseBPTree::delete_p(float key, void** value_out) {
    // Will try to merge, unless we cannot merge the two nodes
}
