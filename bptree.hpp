#pragma once

#include <stddef.h>

class BaseBPTree {
public:
    class Acc;
    struct Node;
    virtual ~BaseBPTree();

    Acc* make_iteration_buffer();
    void destroy_iteration_buffer(Acc* acc);

    // Unit test helpers
    void test_if_values_are_sorted(float since);

protected:
    BaseBPTree();

    void* replace_p(float key, void* value);
    void update_p(float old_key, float new_key, void* value);
    void delete_p(float key, void* match_value);
    void search_p(float key, Acc* out);
    void range_search_p(float k0, float k1, Acc* out);

    virtual void callback(void** buffer, size_t size) = 0;

private:
    Node* root;
};


template<class T>
constexpr bool struct_size_is_appropriate() {
    if (sizeof(void*) == 8) {
        return sizeof(T) % 64 == 0;
    } else {
        return true;
    }
}
