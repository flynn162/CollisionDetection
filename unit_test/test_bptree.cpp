#include <gtest/gtest.h>
#include <math.h>
#include <stdexcept>
#include "../hitbox.hpp"

class MyHitboxes : public HitboxIndex<MyHitboxes> {
public:
    void search_callback(HitboxIterator* iter) {
        while (iter->has_next()) {
            Hitbox* box = iter->next();
            box->a2 = INFINITY;  // mark the hitbox
        }
    }
};

class BPTestFixture : public ::testing::Test {
public:
    // static test data
    static constexpr float keys[] = {1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f};
    static constexpr size_t size = sizeof(keys) / sizeof(float);
    static Hitbox* values;

    bool is_hitbox_marked(float key) {
        for (size_t i = 0; i < this->size; i++) {
            if (this->keys[i] == key) {
                return isinf(this->values[i].a2);
            }
        }
        throw std::logic_error("key not found");
    }

    void reset_test_data() {
        for (size_t i = 0; i < this->size; i++) {
            this->values[i].a1 = (float) i;
            this->values[i].a2 = (float) (i * 2);
            this->values[i].b1 = (float) (i + 1);
            this->values[i].b2 = (float) (2 * (i + 1));
        }
    }

protected:
    void SetUp() override {
        // allocate test data (once)
        if (this->values == nullptr)
            this->values = new Hitbox[this->size];
        // for each test, initialize or reset test data
        this->reset_test_data();
        // for each test, make a new B+Tree
        this->data = new MyHitboxes();
    }

    void TearDown() override {
        delete this->data;
    }

private:
    MyHitboxes* data;
};

Hitbox* BPTestFixture::values = nullptr;


TEST_F(BPTestFixture, InsertionAndRetrieval) {
}

TEST_F(BPTestFixture, RandomInsertionAndRetrieval) {
}

TEST_F(BPTestFixture, InsertingOverlappingKeys) {
}

TEST_F(BPTestFixture, InsertingManyOverlappingKeys) {
}
