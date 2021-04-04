#include <stdint.h>
#include "../TreeNode.h"
#include "common_synopsis.h"

void test_ctor(void** state) {
    TreeNode* node = TreeNode_make();
    // Should be aligned to 64 bytes boundary
    assert_true(0 == ((uintmax_t) node) % 64);
    // Initial weight should be 0
    assert_true(0 == TreeNode_weight(node));
    TreeNode_destroy(node);
}

int test_TreeNode() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ctor)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
