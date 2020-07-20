#pragma once

class BaseBPTree {
public:
    class Acc;
    struct Node;

    Acc* make_iteration_buffer();

protected:
    BaseBPTree();
    virtual ~BaseBPTree();

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
class BPTree : public BaseBPTree {
public:
    T replace(float key, T value) {
        return static_cast<T>(this->replace_p(key, value));
    }
    void update(float old_key, float new_key, T value) {
        this->update_p(old_key, new_key, value);
    }
    void del(float key, T match_value) {
        this->delete_p(key, match_value);
    }
    void search(float key, BaseBPTree::Acc* acc) {
        this->search_p(key, acc);
    }
    void range_search(float k0, float k1, BaseBPTree::Acc* acc) {
        this->range_search_p(k0, k1, acc);
    }
};

template<class T>
constexpr bool struct_size_is_appropriate() {
    if (sizeof(void*) == 8) {
        return sizeof(T) % 64 == 0;
    } else {
        return true;
    }
}
