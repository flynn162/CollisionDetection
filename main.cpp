#include <iostream>
#include "bptree.hpp"

constexpr size_t MAX_LENGTH = 5;

struct Hitbox {
    // W = [a1, b1] X [a2, b2]
    float a1, b1, a2, b2;
};

struct alignas(64) SetNode {
    float label;
    Hitbox* data[MAX_LENGTH];
    SetNode* next;
    SetNode* prev;
};

struct MaybeHitbox {
    union {
        float label;
        Hitbox hb;
        SetNode s;
    };
};

constexpr bool structs_are_aligned() {
    return offsetof(Hitbox, a1) == 0
        && offsetof(SetNode, label) == 0
        && offsetof(MaybeHitbox, label) == 0
        && offsetof(MaybeHitbox, hb) == 0
        && offsetof(MaybeHitbox, s) == 0;
}
static_assert(structs_are_aligned());
static_assert(struct_size_is_appropriate<SetNode>());


class HitboxIndex : public BPTree<Hitbox*> {
protected:
    void callback(void** buffer, size_t size) override {
        std::cout << "cb: size=" << size << "\n";
        for (size_t i = 0; i < size; i++) {
            std::cout << "element " << buffer[i] << "\n";
        }
    }
};

int main() {
    auto bptree = new HitboxIndex();
    auto acc = bptree->make_iteration_buffer();

    Hitbox* hitbox = new Hitbox();
    Hitbox* hitbox2 = new Hitbox();
    std::cout << "inserting " << hitbox << "\n";
    bptree->insert(2.0f, hitbox);
    bptree->insert(3.5f, hitbox);
    std::cout << "inserting " << hitbox2;
    std::cout << " and overriding key 2.0f\n";
    bptree->insert(2.0f, hitbox2);
    bptree->range_search(1.0f, 3.5f, acc);
}
