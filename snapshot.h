#ifndef B_PLUS_TREE_SNAPSHOT_H
#define B_PLUS_TREE_SNAPSHOT_H

#include "B_plus_tree.h"
#include <cstring>
#include <fstream>

#pragma pack(push, 1)

class ID {
public:
    char SnapshotID[17];

    ID(const char SnapshotID_[]) { strcpy(SnapshotID, SnapshotID_); }

    ID() {}

    bool operator<(const ID &obj) const { return strcmp(SnapshotID, obj.SnapshotID) < 0; }
};


class address {
public:
    size_t addr;

    address(size_t addr_) { addr = addr_; }

    address() {}

    bool operator<(const address &obj) const { return addr < obj.addr; }
};

class father {
public:
    size_t fa = 0;
    int ref = 1;

    father(size_t fa_, int ref_) {
        fa = fa_;
        ref = ref_;
    }

    father() {}

    bool operator<(const father &obj) const { return fa < obj.fa; }
};

class get_addr {
public:
    size_t root_addr;

    void find(address addr_) { root_addr = addr_.addr; }

    void not_find() { root_addr = 0; }

    void modify(address &addr_) {}
};

class get_fa {
public:
    class father Father;

    void find(father Father_) { Father = Father_; }

    void not_find() { Father.ref = -1; }

    void modify(father &Father_) { Father_ = Father; }
};

#pragma pack(pop)

//主要用于处理ID-地址对的关系
class snapshot_ID {
    static const int M = 4096 * 5;
private:
    B_plus_tree<ID, address, M, get_addr> ID_to_addr;
public:
    snapshot_ID(char file_name[]) : ID_to_addr(file_name, false) {}

    void insert_ID(char SnapshotID[], size_t addr) {
        try { ID_to_addr.insert(ID(SnapshotID), address(addr)); }
        catch (...) { throw; }
    }

    void erase_ID(char SnapshotID[], size_t addr) {
        try { ID_to_addr.erase(ID(SnapshotID), address(addr)); }
        catch (...) { throw; }
    }

    size_t find_ID(char SnapshotID[]) {
        ID_to_addr.find(ID(SnapshotID));
        return ID_to_addr.Info_operator.root_addr;
    }
};

//主要用于处理父节点和引用数相关信息
class snapshot_father : public base_of_snapshot_father {
    static const int M = 4096 * 5;
private:
    B_plus_tree<address, father, M, get_fa> addr_to_fa;
public:
    snapshot_father(char file_name[]) : addr_to_fa(file_name, false) {}

    void add_addr(size_t addr, size_t fa = 0, int ref = 1) {
        try { addr_to_fa.insert(address(addr), father(fa, ref)); }
        catch (...) { throw unknown_error(); }
    }

    void erase_addr(size_t addr) {
        try {
            addr_to_fa.find(address(addr));
            addr_to_fa.erase(address(addr), addr_to_fa.Info_operator.Father);
        } catch (...) { throw unknown_error(); }
    }

    void change_father(size_t addr, size_t new_father) {
        try {
            addr_to_fa.find(address(addr));
            addr_to_fa.Info_operator.Father.fa = new_father;
            addr_to_fa.modify(address(addr));
        } catch (...) { throw unknown_error(); }
    }

    void change_reference(size_t addr, int change_ref) {
        try {
            addr_to_fa.find(address(addr));
            if (addr_to_fa.Info_operator.Father.ref + change_ref < 0) { throw unknown_error(); }
            addr_to_fa.Info_operator.Father.ref += change_ref;
            addr_to_fa.modify(address(addr));
        } catch (...) { throw unknown_error(); }
    }


    int get_reference(size_t addr) {
        try {
            addr_to_fa.find(address(addr));
            return addr_to_fa.Info_operator.Father.ref;
        } catch (...) { throw unknown_error(); }
    }

    size_t get_father(size_t addr) {
        try {
            addr_to_fa.find(address(addr));
            return addr_to_fa.Info_operator.Father.fa;
        } catch (...) { throw unknown_error(); }
    }

    bool is_exist(size_t addr) {
        try {
            addr_to_fa.find(address(addr));
            return (addr_to_fa.Info_operator.Father.ref != -1);
        } catch (...) { throw unknown_error(); }
    }
};


template<class Key, class Information, int node_size, class find_operator>
class B_plus_snapshot_tree {
private:
    snapshot_ID Snapshot_ID;
    snapshot_father Snapshot_father;
    B_plus_tree<Key, Information, node_size, find_operator> data_tree;

public:
    B_plus_snapshot_tree(char file_name[], char file_ID_name[],
                         char file_fa_name[], bool flag = true) :
            Snapshot_ID(file_ID_name), Snapshot_father(file_fa_name), data_tree(file_name, flag) {
        data_tree.get_Snapshot_father() = &Snapshot_father;
        data_tree.init_father();
    }

    ~B_plus_snapshot_tree() {}

    void create_snapshot(char SnapshotID[]) {
        size_t find_addr_root = Snapshot_ID.find_ID(SnapshotID);
        if (find_addr_root != 0) { throw repeated_ID(); }
        data_tree.create_snapshot();
        Snapshot_ID.insert_ID(SnapshotID, data_tree.get_root_addr());
    }

    void erase_snapshot(char SnapshotID[]) {
        size_t find_addr_root = Snapshot_ID.find_ID(SnapshotID);
        if (find_addr_root == 0) { throw none_exist_ID(); }
        data_tree.erase_snapshot(find_addr_root);
        Snapshot_ID.erase_ID(SnapshotID, find_addr_root);
    }

    void restore_snapshot(char SnapshotID[]) {
        size_t find_addr_root = Snapshot_ID.find_ID(SnapshotID);
        if (find_addr_root == 0) { throw none_exist_ID(); }
        data_tree.restore_snapshot(find_addr_root);
    }

    void find(const Key &key) { data_tree.find(key); }

    void modify(const Key &key) { data_tree.modify(key); }

    void insert(const Key &key, const Information &info) { data_tree.insert(key, info); }

    void erase(const Key &key, const Information &info) { data_tree.erase(key, info); }
};


#endif //B_PLUS_TREE_SNAPSHOT_H
