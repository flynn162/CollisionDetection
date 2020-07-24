#include <gtest/gtest.h>
#include <math.h>
#include <stdexcept>
#include <numeric>
#include <vector>
#include <random>
#include "../hitbox.hpp"

class MyHitboxes : public HitboxIndex<MyHitboxes> {
public:
    virtual ~MyHitboxes() = default;

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
    static float* keys;
    static size_t size;
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

    void perform_range_search() {
        auto acc = this->data->make_iteration_buffer();
        this->data->range_search(1.5f, 2.5f, acc);

        EXPECT_TRUE(this->is_hitbox_marked(1.5f));
        EXPECT_TRUE(this->is_hitbox_marked(2.0f));
        EXPECT_TRUE(this->is_hitbox_marked(2.5f));

        EXPECT_FALSE(this->is_hitbox_marked(1.0f));
        EXPECT_FALSE(this->is_hitbox_marked(3.0f));
        EXPECT_FALSE(this->is_hitbox_marked(3.5f));

        this->data->destroy_iteration_buffer(acc);
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

    MyHitboxes* data;
};

Hitbox* BPTestFixture::values = nullptr;
float* BPTestFixture::keys = new float[6]{1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f};
size_t BPTestFixture::size = 6;


TEST_F(BPTestFixture, InsertionAndRetrieval) {
    for (size_t i = 0; i < this->size; i++) {
        this->data->insert(this->keys[i], &(this->values[i]));
    }
    this->perform_range_search();
}

TEST_F(BPTestFixture, RandomInsertionAndRetrieval) {
    // Shuffle a vec: https://en.cppreference.com/w/cpp/algorithm/iota
    std::vector<size_t> indices(this->size);
    std::iota(indices.begin(), indices.end(), 0);
    auto rng = std::mt19937{std::random_device{}()};
    shuffle(indices.begin(), indices.end(), rng);

    for (size_t i : indices) {
        this->data->insert(this->keys[i], &(this->values[i]));
    }
    this->perform_range_search();
}

TEST_F(BPTestFixture, InsertingOverlappingKeys) {
}

TEST_F(BPTestFixture, InsertingManyOverlappingKeys) {
}
