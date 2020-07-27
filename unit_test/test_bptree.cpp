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

static Hitbox* make_hitbox_array(size_t size) {
    Hitbox* array = new Hitbox[size];
    for (size_t i = 0; i < size; i++) {
        array[i].a1 = i;
        array[i].b1 = 0;
        array[i].a2 = 0;
        array[i].b2 = 0;
    }
    return array;
}

static void EXPECT_ALL_MARKED(Hitbox* array, size_t size) {
    for (size_t i = 0; i < size; i++) {
        EXPECT_TRUE(isinf(array[i].a2)) << "i=" << i;
    }
}

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
    this->data->test_if_root_is_non_degenerate();
}

std::vector<size_t>* make_shuffled_vector(size_t size) {
    auto indices = new std::vector<size_t>(size);
    std::iota(indices->begin(), indices->end(), 0);
    auto rng = std::mt19937{std::random_device{}()};
    shuffle(indices->begin(), indices->end(), rng);
    return indices;
}

TEST_F(BPTestFixture, RandomInsertionAndRetrieval) {
    auto indices = make_shuffled_vector(this->size);

    for (size_t i : *indices) {
        this->data->insert(this->keys[i], &(this->values[i]));
    }
    this->perform_range_search();
    this->data->test_if_root_is_non_degenerate();
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
    EXPECT_ALL_MARKED(this->values, 4);
    this->data->test_if_root_is_non_degenerate();

    this->data->destroy_iteration_buffer(acc);
}

TEST_F(BPTestFixture, InsertingOverlappingKeys2) {
    for (size_t i = 0; i < this->size; i++) {
        this->data->insert(2.0f, &(this->values[i]));
    }
    auto acc = this->data->make_iteration_buffer();
    this->data->range_search(1.5f, 2.5f, acc);
    // check that all hitboxes are marked
    EXPECT_ALL_MARKED(this->values, this->size);
    this->data->test_if_root_is_non_degenerate();
    this->data->destroy_iteration_buffer(acc);
}

TEST(TestBPlusTree, InsertingManyOverlappingKeys) {
    constexpr size_t SIZE = 103;

    Hitbox* array = make_hitbox_array(SIZE);
    auto bptree = new MyHitboxes();
    auto acc = bptree->make_iteration_buffer();

    // insert `SIZE` different hitboxes associated with the same key
    for (size_t i = 0; i < SIZE; i++) {
        bptree->insert(2.0f, &(array[i]));
    }

    // perform a range search and check the markings
    bptree->range_search(1.5f, 2.0f, acc);
    EXPECT_ALL_MARKED(array, SIZE);
    bptree->test_if_root_is_non_degenerate();

    bptree->destroy_iteration_buffer(acc);
    delete bptree;
    delete array;
}

TEST(TestBPlusTree, InsertingManyKeysInReverseOrder) {
    constexpr size_t SIZE = 97;
    Hitbox* array = make_hitbox_array(SIZE);
    auto bptree = new MyHitboxes();
    auto acc = bptree->make_iteration_buffer();
    for (size_t i = 0; i < SIZE; i++) {
        array[i].b1 = 99.0f - i;
        bptree->insert(99.0f - i, &(array[i]));
    }

    bptree->test_if_values_are_sorted(1.0f);
    bptree->range_search(1.0f, 100.0f, acc);
    EXPECT_ALL_MARKED(array, SIZE);
    bptree->test_if_root_is_non_degenerate();

    bptree->destroy_iteration_buffer(acc);
    delete bptree;
    delete array;
}

TEST(TestBPlusTree, RangeSearchInEmptyTree) {
    class DoNotCall : public HitboxIndex<MyHitboxes> {
    public:
        void search_callback(HitboxIterator* iter) {
            throw std::logic_error("should not be called");
        }
    };

    auto bptree = new DoNotCall();
    auto acc = bptree->make_iteration_buffer();
    bptree->range_search(1.5f, 2.5f, acc);
    bptree->destroy_iteration_buffer(acc);
    delete bptree;
}

TEST(TestBPlusTree, RangeSearchInSingleElementTree) {
    Hitbox* hitbox = new Hitbox();
    hitbox->a1 = 1.0f;
    hitbox->b1 = 1.0f;
    hitbox->a2 = 1.0f;
    hitbox->b2 = 1.0f;

    auto bptree = new MyHitboxes();
    auto acc = bptree->make_iteration_buffer();
    bptree->insert(12.0f, hitbox);
    bptree->range_search(11.0f, 12.0f, acc);

    EXPECT_TRUE(isinf(hitbox->a2));  // check if hitbox is marked
    bptree->test_if_root_is_non_degenerate();

    bptree->destroy_iteration_buffer(acc);
    delete bptree;
    delete hitbox;
}
