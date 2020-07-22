#include <iostream>
#include <math.h>
#include "bptree.hpp"
#include "hitbox.hpp"

constexpr size_t NODE_DATA_SIZE = 6;
constexpr size_t HEADER_DATA_SIZE = 5;


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

enum State : unsigned char {
    IN_BUFFER,
    IN_SET_HEADER,
    IN_SET_NODE,
    ITERATOR_ENDED
};

enum Alphabet : int {
    CHAR_ELEMENT = 'A',
    CHAR_OPEN = '(',
    CHAR_CLOSE = ')',
    CHAR_COLON = ':',
    CHAR_ARROW = '>',
    CHAR_EOF = '$'
};

HitboxIterator::HitboxIterator(void** buffer, size_t size) {
    // store the info
    this->buffer = buffer;
    this->size = size;
    // this slot is shared among different states; need initialization
    this->holding_slot = nullptr;
    // only need to initialize the IN_BUFFER part of this struct
    this->index_buf = 0;
    this->state = IN_BUFFER;
}

void HitboxIterator::to_state_in_set_header(void* pointer) {
    this->state = IN_SET_HEADER;
    this->pointer = pointer;
    auto header = static_cast<SetHeader*>(pointer);
    this->length_of_last_node = header->length_of_last_node;
    if (header->first == nullptr) {
        this->counter = this->length_of_last_node;
    } else {
        this->counter = HEADER_DATA_SIZE;
    }
}

void HitboxIterator::to_state_in_buffer() {
    this->state = IN_BUFFER;
    this->index_buf++;
}

void HitboxIterator::to_state_in_set_node(void* pointer) {
    this->state = IN_SET_NODE;
    this->pointer = pointer;
    auto node = static_cast<SetNode*>(pointer);
    if (node->next == nullptr) {
        this->counter = this->length_of_last_node;
    } else {
        this->counter = NODE_DATA_SIZE;
    }
}

int HitboxIterator::generate_input() {
    if (this->state == IN_BUFFER) {
        if (this->index_buf == this->size) {
            this->state = ITERATOR_ENDED;
            return CHAR_EOF;
        }

        auto maybe = (MaybeHitbox*) (this->buffer[this->index_buf]);
        if (isnan(maybe->label)) {
            // set header
            this->to_state_in_set_header(&(maybe->s));
            return CHAR_OPEN;
        } else {
            // hitbox
            this->holding_slot = &(maybe->hb);
            this->to_state_in_buffer();
            return CHAR_ELEMENT;
        }
    }

    else if (this->state == IN_SET_HEADER) {
        auto header = static_cast<SetHeader*>(this->pointer);
        if (counter > 0) {
            // iterate through remaining elements in the header
            this->holding_slot = header->data[counter - 1];
            this->counter--;
            return CHAR_ELEMENT;
        } else if (header->first == nullptr) {
            // there are not set nodes
            this->to_state_in_buffer();
            return CHAR_CLOSE;
        } else {
            // go to the first set node
            this->to_state_in_set_node(header->first);
            return CHAR_COLON;
        }
    }

    else if (this->state == IN_SET_NODE) {
        auto node = static_cast<SetNode*>(this->pointer);
        if (this->counter > 0) {
            // iterate through remaining elements in the node
            this->holding_slot = node->data[counter - 1];
            this->counter--;
            return CHAR_ELEMENT;
        } else if (node->next == nullptr) {
            // this is the last node
            this->to_state_in_buffer();
            return CHAR_CLOSE;
        } else {
            // go to the next node
            this->to_state_in_set_node(node->next);
            return CHAR_ARROW;
        }
    }

    else {
        return CHAR_EOF;
    }
}

bool HitboxIterator::has_next() {
    auto input = this->generate_input();
    while (input != CHAR_EOF) {
        if (input == CHAR_ELEMENT)
            return true;
        else
            input = this->generate_input();
    }
    return false;
}


using super = BPTree<HitboxIndex, HitboxIndexTypes>;
using Acc = BaseBPTree::Acc;

void HitboxIndex::insert(float key, Hitbox* value) {
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

void HitboxIndex::update(float old_key, float new_key, Hitbox* value) {
    // not implemented
}

void HitboxIndex::del(float key, Hitbox* match_value) {
    // not implemented
}

void HitboxIndex::ball_query(float mag, float rad, float R, Acc* acc) {
    float temp = rad + R;
    this->range_search(mag - temp, mag + temp, acc);
}

void HitboxIndex::search_callback(HitboxIterator* iter) {
    std::cout << "search callback: ";
    while (iter->has_next()) {
        std::cout << iter->next() << " ";
    }
    std::cout << "\n";
}

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
