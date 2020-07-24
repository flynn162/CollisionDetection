#include <gtest/gtest.h>
#include <math.h>
#include <stdexcept>
#include <numeric>
#include <vector>
#include <random>
#include "../hitbox.hpp"

class MyHitboxes : public HitboxIndex<MyHitboxes> {
public:
    void search_callback(HitboxIterator* iter) {
        while (iter->has_next()) {
            Hitbox* box = iter->next();
            if (isinf(box->a2))
                throw std::logic_error("hitbox is already marked");
            else
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
    // key=2.0f
    this->data->insert(2.0f, &(this->values[0]));
    this->data->insert(2.0f, &(this->values[1]));
    this->data->insert(2.0f, &(this->values[2]));
    // key=1.5f
    this->data->insert(1.5f, &(this->values[3]));
    this->data->insert(1.5f, &(this->values[4]));

    auto acc = this->data->make_iteration_buffer();
    this->data->range_search(1.0f, 2.0f, acc);

    // check that hitboxes 0--4 are marked
    for (size_t i = 0; i < 4; i++) {
        ASSERT_TRUE(isinf(this->values[i].a2));
    }

    this->data->destroy_iteration_buffer(acc);
}

TEST_F(BPTestFixture, InsertingOverlappingKeys2) {
    for (size_t i = 0; i < this->size; i++) {
        this->data->insert(2.0f, &(this->values[i]));
    }
    auto acc = this->data->make_iteration_buffer();
    this->data->range_search(1.5f, 2.5f, acc);
    // check that all hitboxes are marked
    for (size_t i = 0; i < this->size; i++) {
        ASSERT_TRUE(isinf(this->values[i].a2));
    }
    this->data->destroy_iteration_buffer(acc);
}

TEST(TestBPlusTree, InsertingManyOverlappingKeys) {
    class Counter : public HitboxIndex<Counter> {
    public:
        void search_callback(HitboxIterator* iter) {
            while (iter->has_next()) {
                auto hitbox = iter->next();
                hitbox->a2 += 1.0f;
            }
        }
    };

    Hitbox* hitbox = new Hitbox();
    hitbox->a1 = 0;
    hitbox->b1 = 0;
    hitbox->a2 = 0;
    hitbox->b2 = 0;

    auto bptree = new Counter();
    auto acc = bptree->make_iteration_buffer();

    // insert the same hitbox 71 times, using the same key
    for (size_t i = 0; i < 71; i++) {
        bptree->insert(2.0f, hitbox);
    }
    // perform a range search and count the hits
    bptree->range_search(1.5f, 2.5f, acc);
    EXPECT_TRUE(hitbox->a2 >= 70.5f && hitbox->a2 <= 71.5f);

    bptree->destroy_iteration_buffer(acc);
    delete bptree;
    delete hitbox;
}
