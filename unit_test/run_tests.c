void acc(int* original, int new_value) {
    if (new_value > 0) {
        *original = new_value;
    }
}

// Test file entry points
int test_TreeNode();
int test_Collection();

int main() {
    int result = 0;
    acc(&result, test_TreeNode());
    acc(&result, test_Collection());
    return result;
}
