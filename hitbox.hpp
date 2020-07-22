#pragma once

#include "bptree.hpp"

struct Hitbox {
    // W = [a1, b1] X [a2, b2]
    float a1, b1, a2, b2;
};

class HitboxIterator {
public:
    HitboxIterator(void** buffer, size_t size);
    bool has_next();

    // The obvious
    ~HitboxIterator() = default;
    Hitbox* next() {
        auto result = this->holding_slot;
        this->holding_slot = nullptr;
        return result;
    }

private:
    // restrict heap allocation
    void* operator new(size_t size) = delete;
    void to_state_in_set_header(void* pointer);
    void to_state_in_buffer();
    void to_state_in_set_node(void* pointer);
    int generate_input();

    void** buffer;
    size_t size;
    Hitbox* holding_slot;
    void* pointer;
    size_t index_buf;
    unsigned char counter;
    unsigned char length_of_last_node;
    unsigned char state;
};

class HitboxIndexTypes {
public:
    // CRTP-required information
    using element_type = Hitbox*;
    using iterator_type = HitboxIterator;
};

class HitboxIndex : public BPTree<HitboxIndex, HitboxIndexTypes> {
public:
    void insert(float key, Hitbox* value);
    void update(float old_key, float new_key, Hitbox* value);
    void del(float key, Hitbox* match_value);
    void ball_query(float mag, float rad, float R, BaseBPTree::Acc* acc);

protected:
    // temporary
    friend class BPTree<HitboxIndex, HitboxIndexTypes>;
    void search_callback(HitboxIterator* iter);
};
