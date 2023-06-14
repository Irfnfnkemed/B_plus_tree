#include "B_plus_tree.h"
#include <iostream>
#include <cstring>
#include "snapshot.h"

class KEY {
public:
    char key[65];

    KEY(const char key_[]) { strcpy(key, key_); }

    KEY() {}

    bool operator<(const KEY &obj) const { return strcmp(key, obj.key) < 0; }
};

class VALUE {
public:
    int value;

    VALUE(int value_) { value = value_; }

    VALUE() {}

    bool operator<(const VALUE &obj) const { return value < obj.value; }
};

class find_operator {
public:
    class VALUE find_value;

    void find(VALUE find_value_) {
        find_value = find_value_;
        std::cout << find_value.value << ' ';
    }

    void not_find() { std::cout << "null"; }

    void modify(VALUE &value) { find_value = value; }
};


const int MAX_SIZE = 4096 * 5;


int main() {
    B_plus_snapshot_tree<KEY, VALUE, MAX_SIZE, find_operator> a("data", "ID", "father");
    int n;
    scanf("%d", &n);
    char tmp[64];
    int value;
    for (int i = 1; i <= n; ++i) {
        scanf("%s", tmp);
        try {
            if (strcmp("insert", tmp) == 0) {
                scanf("%s%d", tmp, &value);
                a.insert(KEY(tmp), VALUE{value});
            } else if (strcmp("delete", tmp) == 0) {
                scanf("%s%d", tmp, &value);
                a.erase(KEY(tmp), VALUE{value});
            } else if (strcmp("find", tmp) == 0) {
                scanf("%s", tmp);
                a.find(KEY(tmp));
                std::cout << std::endl;
            } else if (strcmp("create_snapshot", tmp) == 0) {
                scanf("%s", tmp);
                a.create_snapshot(tmp);
            } else if (strcmp("erase_snapshot", tmp) == 0) {
                scanf("%s", tmp);
                a.erase_snapshot(tmp);
            } else if (strcmp("restore_snapshot", tmp) == 0) {
                scanf("%s", tmp);
                a.restore_snapshot(tmp);
            }
        } catch (exception &e) {
            std::cout << e.what() << std::endl;
        }
    }
    return 0;
}
