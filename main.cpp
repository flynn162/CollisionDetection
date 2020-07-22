#include <iostream>
#include "hitbox.hpp"

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
    std::cout << "range search: ||y|| in [1, 3.5]\n";
    bptree->range_search(1.0f, 3.5f, acc);
    std::cout << "ball query: ||x||=1, r=1, R=0.5\n";
    bptree->ball_query(1.0f, 1.0f, 0.5f, acc);
}
