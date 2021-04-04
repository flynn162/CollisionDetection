#include <stdint.h>
#include "../Collection.h"
#include "common_synopsis.h"

static int set_up(void** state) {
    Collection* collection = Collection_make(NULL);  // 1st element: NULL
    *state = collection;
    return 0;
}

static int tear_down(void** state) {
    Collection_destroy((Collection*) *state);
    return 0;
}

static void test_ctor(void** state) {
    Collection* collection = (Collection*) *state;
    // Should be aligned to 64 bytes boundary
    assert_int_equal(0, ((uintmax_t) collection) % 64);
    // Initial "last node size" should be 1
    // This is because the collection is non-empty after construction
    assert_int_equal(1, Collection_last_node_size(collection));
}

static void test_nan_decoding(void** state) {
    Collection* collection = (Collection*) *state;
    Collection_set_last_node_size(collection, 3);
    assert_int_equal(3, Collection_last_node_size(collection));
    Collection_set_last_node_size(collection, 0);
    assert_int_equal(0, Collection_last_node_size(collection));
}

static void test_add_updates_last_node_size(void** state) {
    int data[] = {10, 20};
    Collection* collection = (Collection*) *state;
    assert_true(SET_HEADER_DATA_SIZE >= 3);
    Collection_add(collection, &(data[0]));
    assert_int_equal(2, Collection_last_node_size(collection));
    Collection_add(collection, &(data[1]));
    assert_int_equal(3, Collection_last_node_size(collection));
}

#define add_test(X) cmocka_unit_test_setup_teardown(X, set_up, tear_down)

int test_Collection() {
    const struct CMUnitTest tests[] = {
        add_test(test_ctor),
        add_test(test_nan_decoding),
        add_test(test_add_updates_last_node_size),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
