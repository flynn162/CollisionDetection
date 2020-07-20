#include <iostream>
#include <math.h>
#include "bptree.hpp"

constexpr size_t NODE_DATA_SIZE = 6;
constexpr size_t HEADER_DATA_SIZE = 5;

struct Hitbox {
    // W = [a1, b1] X [a2, b2]
    float a1, b1, a2, b2;
};

struct alignas(64) SetNode {
    Hitbox* data[NODE_DATA_SIZE];
    SetNode* next;
    SetNode* prev;
};

struct alignas(64) SetHeader {
    float label;
    unsigned char length_of_last_node;
    SetNode* first;
    SetNode* last;
    Hitbox* data[HEADER_DATA_SIZE];
};

union MaybeHitbox {
    float label;
    Hitbox hb;
    SetHeader s;
};

constexpr bool structs_are_aligned() {
    return offsetof(Hitbox, a1) == 0
        && offsetof(SetHeader, label) == 0
        && offsetof(MaybeHitbox, label) == 0
        && offsetof(MaybeHitbox, hb) == 0
        && offsetof(MaybeHitbox, s) == 0;
}
static_assert(structs_are_aligned());
static_assert(struct_size_is_appropriate<SetNode>());
static_assert(struct_size_is_appropriate<SetHeader>());


static SetHeader* make_set_header(Hitbox* initial_element) {
    auto result = new SetHeader();
    result->label = NAN;
    result->length_of_last_node = 1;
    result->first = nullptr;
    result->last = nullptr;
    result->data[0] = initial_element;
    return result;
}

static SetNode* make_set_node(Hitbox* value, SetNode* next, SetNode* prev) {
    auto result = new SetNode();
    result->next = next;
    result->prev = prev;
    result->data[0] = value;
    return result;
}

static void delete_set_header(SetHeader* self) {
    delete self;
}

static void delete_set_node(SetNode* self) {
    delete self;
}

static void add(SetHeader* self, Hitbox* value) {
    if (self->last == nullptr) {
        if (self->length_of_last_node < HEADER_DATA_SIZE) {
            self->data[self->length_of_last_node] = value;
            self->length_of_last_node++;
        } else {
            auto new_node = make_set_node(value, nullptr, nullptr);
            self->length_of_last_node = 1;
            self->first = new_node;
            self->last = new_node;
        }
    } else {
        if (self->length_of_last_node < NODE_DATA_SIZE) {
            self->last->data[self->length_of_last_node] = value;
            self->length_of_last_node++;
        } else {
            auto new_node = make_set_node(value, nullptr, self->last);
            self->last->next = new_node;
            self->last = new_node;
            self->length_of_last_node = 1;
        }
    }
}

static bool is_singleton(SetHeader* self) {
    return self->last == nullptr && self->length_of_last_node == 1;
}

static bool is_empty(SetHeader* self) {
    return self->last == nullptr && self->length_of_last_node == 0;
}

template<class T, class V>
static int find_in_node(T* self, V value, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (self->data[i] == value) {
            return i;
        }
    }
    return -1;
}

static SetNode* find(SetHeader* self, Hitbox* value, size_t* idxout) {
    // Find the set node and the in-node index of a value. Return a pointer to
    // the set node that contains the value. If value is in the header, return
    // null. The index is always written to idxout.
    //
    // Behavior is undefined if the value is not in the set.

    size_t size = HEADER_DATA_SIZE;
    if (self->last == nullptr)
        size = self->length_of_last_node;

    // iterate through the header first
    int idx;
    idx = find_in_node(self, value, size);
    if (idx >= 0) {
        *idxout = idx;
        return nullptr;
    }

#ifdef DEBUG
    if (self->last == nullptr)
        throw std::logic_error("value not found");
#endif

    // iterate through set nodes
    SetNode* curr = self->first;
    while (curr->next != nullptr) {
        idx = find_in_node(curr, value, NODE_DATA_SIZE);
        if (idx >= 0) {
            *idxout = idx;
            return curr;
        }
    }
    // last set node
    idx = find_in_node(curr, value, self->length_of_last_node);
#ifdef DEBUG
    if (idx == -1)
        throw std::logic_error("value not found");
#endif
    *idxout = idx;
    return curr;
}

static void del(SetHeader* self, Hitbox* value) {
    // Delete a value from the set
    // Behavior is undefined if the value is not in the set

    // find the value
    size_t idx = 0;
    SetNode* node = find(self, value, &idx);

    // fill the gap
    if (self->last == nullptr) {
        // only need to modify the header
        self->data[idx] = self->data[self->length_of_last_node - 1];
        self->length_of_last_node--;
    } else {
        // move the last value into the gap
        Hitbox* new_value = self->last->data[self->length_of_last_node - 1];
        if (node == nullptr)
            self->data[idx] = new_value;
        else
            node->data[idx] = new_value;
        // shrink node or delete node
        if (self->length_of_last_node == 1) {
            auto to_be_freed = self->last;
            self->last = self->last->prev;
            if (self->last == nullptr)
                self->first = nullptr;
            else
                self->last->next = nullptr;
            delete_set_node(to_be_freed);
            self->length_of_last_node = NODE_DATA_SIZE;
        } else {
            self->length_of_last_node--;
        }
    }
}

class HitboxIndex : public BPTree<Hitbox*> {
public:
    void insert(float key, Hitbox* value) {
        auto maybe = (MaybeHitbox*) (this->super::replace(key, value));
        if (maybe != nullptr) {
            // Something got replaced. Need to re-add
            if (isnan(maybe->label)) {
                // it is a set that got replaced
                add(&(maybe->s), value);
                // make the type system happy
                this->super::replace(key, &(maybe->hb));
            } else {
                // it is hitbox that got replaced
                auto new_set = make_set_header(&(maybe->hb));
                add(new_set, value);
                // make the type system happy
                auto ptr = (MaybeHitbox*) new_set;
                this->super::replace(key, &(ptr->hb));
            }
        }
    }
    void update(float old_key, float new_key, Hitbox* value) {
        // not implemented
    }
    void del(float key, Hitbox* match_value) {
        // not implemented
    }

protected:
    void callback(void** buffer, size_t size) override {
        std::cout << "cb: size=" << size << "\n";
        for (size_t i = 0; i < size; i++) {
            std::cout << "element " << buffer[i];
            MaybeHitbox* maybe = static_cast<MaybeHitbox*>(buffer[i]);
            if (isnan(maybe->label)) {
                std::cout << " is not a hitbox (it is a set)\n";
            } else {
                std::cout << " is a hitbox\n";
            }
        }
    }
private:
    using super = BPTree<Hitbox*>;
};

int main() {
    auto bptree = new HitboxIndex();
    auto acc = bptree->make_iteration_buffer();

    Hitbox* hitbox = new Hitbox();
    Hitbox* hitbox2 = new Hitbox();
    hitbox->a1 = 100.0f;
    hitbox2->a1 = 200.0f;
    std::cout << "inserting " << hitbox << " with key 2.0f\n";
    bptree->insert(2.0f, hitbox);
    std::cout << "inserting " << hitbox2 << " overriding key 2.0f\n";
    bptree->insert(2.0f, hitbox2);
    bptree->range_search(1.0f, 3.5f, acc);
}
