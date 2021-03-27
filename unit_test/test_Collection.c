#include <stdint.h>
#include "../Collection.h"
#include "common_synopsis.h"

static void test_ctor(void** state) {
    Collection* collection = Collection_make(NULL);  // 1st element: NULL
    // Should be aligned to 64 bytes boundary
    assert_int_equal(0, ((uintmax_t) collection) % 64);
    // Initial "last node size" should be 1
    // This is because the collection is non-empty after construction
    assert_int_equal(1, Collection_last_node_size(collection));
    Collection_destroy(collection);
}

int test_Collection() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ctor)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
