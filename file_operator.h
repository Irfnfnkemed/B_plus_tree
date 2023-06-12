#ifndef B_PLUS_TREE_FILE_OPERATOR_H
#define B_PLUS_TREE_FILE_OPERATOR_H

#include <fstream>
#include <iostream>
#include "exceptions.h"

class base_of_snapshot_father {
public:
    base_of_snapshot_father() {}

    ~base_of_snapshot_father() {}

    virtual void erase_addr(size_t addr) = 0;

    virtual void change_addr(size_t old_addr, size_t new_addr) = 0;

    virtual void change_father(size_t addr, size_t new_father) = 0;

    virtual void change(size_t, size_t, int) = 0;

    virtual void add_addr(size_t addr, size_t fa = 0, int ref = 1) = 0;

    virtual void change_reference(size_t addr, int change_ref) = 0;

    virtual int get_reference(size_t addr) = 0;

    virtual int get_now_reference() = 0;

    virtual size_t get_father(size_t addr) = 0;
};

template<class cache_node, int max>
class hash_link {//实现从size_t到cache_node的散列表
private:
    struct node {
        size_t address;
        cache_node *to;
        node *next;

        node(size_t address_, cache_node *to_, node *next_) {
            address = address_;
            to = to_;
            next = next_;
        }
    };

    node *link[max];//用取模来实现散列
public:
    hash_link() { for (int i = 0; i < max; ++i) { link[i] = nullptr; }}

    ~hash_link() {
        node *p;
        for (int i = 0; i < max; ++i) {
            if (link[i] == nullptr) { continue; }
            else {
                while (true) {
                    p = link[i];
                    if (p == nullptr) { break; }
                    link[i] = link[i]->next;
                    delete p;
                }
            }
        }
    }

    void insert(size_t addr, cache_node *addr_to) {
        link[addr % max] = new node(addr, addr_to, link[addr % max]);
    }

    void erase(size_t addr) {
        node *p_pre = nullptr, *p = link[addr % max];
        while (p != nullptr) {
            if (p->address == addr) {
                if (p_pre != nullptr) { p_pre->next = p->next; }
                else { link[addr % max] = p->next; }
                delete p;
                return;
            } else {
                p_pre = p;
                p = p->next;
            }
        }
    }

    cache_node *find(size_t addr) {
        node *p = link[addr % max];
        while (p != nullptr) {
            if (p->address == addr) {
                return p->to;
            } else { p = p->next; }
        }
        return nullptr;
    }
};


class base_of_cache {
public:
    virtual void change_son(size_t, size_t, size_t, std::fstream &, size_t &) = 0;
};

template<class content, int max>
class cache : public base_of_cache {//实现缓冲区读写，缓存content类信息块
protected:
    struct cache_node {
        size_t address;
        cache_node *next;
        cache_node *pre;
        content *to;
        bool modify = false;

        cache_node(size_t address_ = 0, cache_node *next_ = nullptr,
                   cache_node *pre_ = nullptr, content *to_ = nullptr) {
            address = address_;
            next = next_;
            pre = pre_;
            if (next != nullptr) { next->pre = this; }
            if (pre != nullptr) { pre->next = this; }
            to = to_;
            modify = false;
        }
    };

    int size;
    cache_node *head;
    cache_node *tail;
    hash_link<cache_node, 49999> random_access;
    base_of_snapshot_father *Snapshot_father = nullptr;
    bool break_size = false;//为真，表示可以无视size的限制
    base_of_cache *cache_fa = nullptr;

    //将addr位置的节点的addr_old_son儿子改为addr_new_son
    void change_son(size_t addr, size_t addr_old_son, size_t addr_new_son,
                    std::fstream &file, size_t &root) {
        content *p = get(addr, file, root);
        for (int i = 0; i <= p->number; ++i) {
            if (p->get_addr(i) == addr_old_son) {
                Snapshot_father->change_reference(p->get_addr(i), -1);
                p->get_addr(i) = addr_new_son;//改变指向
                break;
            }
        }
        set_modify(addr, file, root);
    }

    void adjust(cache_node *now) {//将节点提到link尾部
        now->pre->next = now->next;
        now->next->pre = now->pre;
        tail->pre->next = now;
        now->pre = tail->pre;
        tail->pre = now;
        now->next = tail;
    }


    void erase(cache_node *now) {//删除当前节点(节点对应文件空间已释放，不能再写入文件中)
        random_access.erase(now->address);//删除hash_link对应内容
        now->pre->next = now->next;
        now->next->pre = now->pre;
        delete now->to;
        delete now;
        --size;
    }

    cache_node *get_cache_node(size_t address, std::fstream &file, size_t &root) {
        //访问address对应的文件内容
        //若缓存中已经存储了该地址对应信息，则返回对应内容，并将其提到link尾部
        //若缓存中未存储该地址对应信息，则文件中读取，插到link尾部，返回对应内容
        //若在插入缓存时，发现缓存已满，则弹出link头部的内容，并更新到文件中
        cache_node *p = random_access.find(address);
        if (p == nullptr) {
            bool flag = break_size;
            set_break_size(true, file, root);
            p = new cache_node(address, tail, tail->pre, new content);
            file.seekg(address, std::ios::beg);
            file.read(reinterpret_cast<char *>(p->to), sizeof(content));
            random_access.insert(address, p);
            ++size;
            set_break_size(flag, file, root);
        } else { adjust(p); }
        return p;
    }


public:

    //store_root为真，表示文件开头存储根节点位置;反之，不存储
    cache() {
        Snapshot_father = nullptr;
        break_size = false;
        head = new cache_node;
        tail = new cache_node;
        head->next = tail;
        tail->pre = head;
        size = 0;
    }

    void clear(std::fstream &file, size_t &root, cache *key_cache = nullptr) {
        break_size = true;
        while (head->next != tail) { pop(file, root); }
        delete head;
        delete tail;
        break_size = false;
    }

    ~cache() {}

    //弹出指定节点(默认链表头部)，并更新对应文件
    size_t pop(std::fstream &file, size_t &root, size_t addr = 0) {
        cache_node *p;
        if (addr == 0) { p = head->next; }
        else { p = get_cache_node(addr, file, root); }
        size_t addr_now = p->address;
        if (Snapshot_father == nullptr) {//无快照情况
            if (p->modify) {
                file.seekg(p->address, std::ios::beg);
                file.write(reinterpret_cast<char *>(p->to), sizeof(content));//将缓存的内容存入文件中
            }
            erase(p);
        } else {//有快照
            int ref = Snapshot_father->get_reference(p->address);
            if (ref == 1) {//直接写入即可
                if (p->modify) {
                    file.seekg(p->address, std::ios::beg);
                    file.write(reinterpret_cast<char *>(p->to), sizeof(content));//将缓存的内容存入文件中
                }
                erase(p);
            } else {
                file.seekg(0, std::ios::end);
                size_t addr_father, addr_new_now = file.tellg();
                addr_now = addr_new_now;
                if (cache_fa == nullptr) {//需要复制到新节点，并且更新父节点，同时更新各节点的儿子的父亲指向，注意引用数的变化
                    file.write(reinterpret_cast<char *>(p->to), sizeof(content));//将缓存内容存入文件末尾的新空间中
                    for (int i = 0; i <= p->to->number; ++i) {//改变儿子的父亲指向
                        Snapshot_father->change(p->to->get_addr(i), addr_new_now, 1);
                    }
                    addr_father = Snapshot_father->get_father(p->address);
                    if (addr_father == 0) {//当前为根，创建了新根
                        root = p->address;
                        Snapshot_father->add_addr(addr_new_now, 0);//更新新根
                        Snapshot_father->change_reference(p->address, -1);//更新原根
                    } else { change_son(addr_father, p->address, addr_new_now, file, root); }//更新父节点
                    erase(p);
                } else {
                    file.write(reinterpret_cast<char *>(p->to), sizeof(p));//将缓存的内容存入文件中
                    addr_father = Snapshot_father->get_father(p->address);
                    Snapshot_father->add_addr(addr_new_now, addr_father);
                    cache_fa->change_son(addr_father, addr_new_now, p->address, file, root);
                    erase(p);
                }
            }
        }
        return addr_now;
    }

    inline base_of_snapshot_father *&get_Snapshot_father() { return Snapshot_father; }

    content *get(size_t address, std::fstream &file, size_t &root) {
        return get_cache_node(address, file, root)->to;
    }

    void free_memory(size_t address) {//释放(若存在)对应的缓存，不考虑对其父亲儿子的影响
        cache_node *p = random_access.find(address);
        if (p != nullptr) {//若缓存中有
            erase(p);//删除节点
        }
    }

    inline void set_break_size(bool flag, std::fstream &file, size_t &root) {
        if (!flag) { while (size > max) { pop(file, root); }}
        break_size = flag;
    }

    inline base_of_cache *&get_cache_fa() { return cache_fa; }

    void set_modify(size_t address, std::fstream &file, size_t &root) {
        get_cache_node(address, file, root)->modify = true;
    }

};

template<class key_node, class info_node>
class files {
private:
    cache<key_node, 10> cache_key;//key相关的缓存
    cache<info_node, 10> cache_info;//info相关的缓存
    size_t key_root;//根的位置
    size_t free_head;//第一个空闲节点的位置
    std::fstream file;
    base_of_snapshot_father *Snapshot_father = nullptr;

    inline bool open_file(char file_name[]) {
        file.open(file_name, std::ios::in);
        if (file.is_open()) {
            file.close();
            file.open(file_name, std::ios::in | std::ios::out | std::ios::binary);
            return true;
        } else {
            file.open(file_name, std::ios::out);
            file.close();
            file.open(file_name, std::ios::in | std::ios::out | std::ios::binary);
            return false;
        }
    }

public:

    files(char file_name[]) {//初始化各个部分
        if (!open_file(file_name)) {//创建初始空根节点
            key_root = 2 * sizeof(size_t);//初始时，第一个位置作为根节点
            free_head = 0;//0表明此时没有空闲空间
            file.seekg(0, std::ios::beg);
            file.write(reinterpret_cast<char *>(&key_root), sizeof(size_t));
            file.write(reinterpret_cast<char *>(&free_head), sizeof(size_t));
            get_addr(0);//创建空的根节点
            set_modify(0, key_root);
            file.seekg(0, std::ios::end);
            get_addr(1);//创建空的第一个信息节点
            set_modify(1, sizeof(key_node) + 2 * sizeof(size_t));
        } else {
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char *>(&key_root), sizeof(size_t));
            file.read(reinterpret_cast<char *>(&free_head), sizeof(size_t));
        }
        Snapshot_father = nullptr;
        cache_info.get_cache_fa() = &cache_key;
    }

    ~files() {
        file.seekg(0, std::ios::beg);
        file.write(reinterpret_cast<char *>(&key_root), sizeof(size_t));//更新根节点位置
        file.write(reinterpret_cast<char *>(&free_head), sizeof(size_t));//更新第一个空闲节点位置
        cache_key.clear(file, key_root);
        cache_info.clear(file, key_root);
        file.close();
    }

    inline base_of_snapshot_father *&get_Snapshot_father() { return Snapshot_father; }

    void init_father() {
        cache_key.get_Snapshot_father() = Snapshot_father;
        cache_info.get_Snapshot_father() = Snapshot_father;
    }

    inline key_node *get_key(size_t address) { return cache_key.get(address, file, key_root); }

    inline info_node *get_info(size_t address) { return cache_info.get(address, file, key_root); }

    size_t get_addr(int cat) {
        //给出一块可行空间的地址(分配空间)
        //若有相应的空闲空间，那么返回对应起始地址，同时随之更新相关信息
        //若无有相应的空闲空间，那么直接在相应文件开新的空间，并返回对应起始地址
        //cat为0，表示对key节点进行操作；反之，对info节点进行操作
        if (free_head == 0) {//开辟一块新的空间
            file.seekg(0, std::ios::end);
            if (!cat) {
                key_node tmp;
                file.write(reinterpret_cast<char *>(&tmp), sizeof(key_node));//生成空白区域
                return size_t(file.tellg()) - sizeof(key_node);
            } else {
                info_node tmp;
                file.write(reinterpret_cast<char *>(&tmp), sizeof(info_node));//生成空白区域
                return size_t(file.tellg()) - sizeof(info_node);
            }
        } else {
            file.seekg(free_head, std::ios::beg);
            file.read(reinterpret_cast<char *>(&free_head), sizeof(size_t));
            return size_t(file.tellg()) - sizeof(size_t);
        }
    }

    void free_addr(size_t address, int cat) {
        //释放对应的空间
        //cat为0，表示对key节点进行操作；反之，对info节点进行操作
        set_break_size(true);
        if (Snapshot_father != nullptr) {
            if (Snapshot_father->get_reference(address) >= 1) {//只需从缓存中删去即可
                if (!cat) { cache_key.free_memory(address); }
                else { cache_info.free_memory(address); }
            } else {//需要彻底释放该块
                file.seekg(address, std::ios::beg);
                file.write(reinterpret_cast<char *>(&free_head), sizeof(size_t));//更新空闲节点形成的链表
                free_head = address;
                if (!cat) { cache_key.free_memory(address); }
                else { cache_info.free_memory(address); }
                Snapshot_father->erase_addr(address);
            }
        } else {//直接释放
            file.seekg(address, std::ios::beg);
            file.write(reinterpret_cast<char *>(&free_head), sizeof(size_t));//更新空闲节点形成的链表
            free_head = address;
            if (!cat) { cache_key.free_memory(address); }
            else { cache_info.free_memory(address); }
        }
        set_break_size(false);
    }

    inline size_t get_root_addr() { return key_root; }

    inline void update_root_addr(size_t addr) { key_root = addr; }

    //cat为0，表key；反之，表info
    void set_modify(int cat, size_t address) {
        if (!cat) { cache_key.set_modify(address, file, key_root); }
        else { cache_info.set_modify(address, file, key_root); }
    }

    void set_break_size(bool flag) {
        cache_key.set_break_size(flag, file, key_root);
        cache_info.set_break_size(flag, file, key_root);
    }

    //cat为0，表示当前为key节点；为1，表为info节点
    void set_snapshot(size_t addr, int cat = 0) {
        if (Snapshot_father == nullptr) { throw unknown_error(); }
        if (!cat) {
            key_node *p = cache_key.get(addr, file, key_root);
            for (int i = 0; i <= p->number; ++i) { set_snapshot(p->address[i], p->is_leaf); }
            size_t addr_now = cache_key.pop(file, key_root, addr);
            Snapshot_father->change_reference(addr_now, 1);//当前引用加1
        } else {
            info_node *p = cache_info.get(addr, file, key_root);
            size_t addr_now = cache_info.pop(file, key_root, addr);
            Snapshot_father->change_reference(addr_now, 1);
        }
    }

    //在快照节点为根的树中，每个节点的引用数均减少1，若有某个块被彻底释放，还要考虑其连锁效应
    void erase_snapshot(size_t addr, int cat = 0, int is_fa_free = 0) {
        if (Snapshot_father == nullptr) { throw unknown_error(); }
        if (!cat) {
            key_node *p = cache_key.get(addr, file, key_root);
            int is_now_free = 0;
            if (Snapshot_father->get_reference(addr) == 1 + is_fa_free) {
                is_now_free = 1;//当前块将被释放
            }
            for (int i = 0; i <= p->number; ++i) {
                erase_snapshot(p->address[i], p->is_leaf, is_now_free);//释放儿子
            }
            Snapshot_father->change_reference(addr, -(1 + is_fa_free));//修改引用数
            free_addr(addr, 0);//释放该块
        } else {
            info_node *p = cache_info.get(addr, file, key_root);
            Snapshot_father->change_reference(addr, -(1 + is_fa_free));//修改引用数
            free_addr(addr, 1);//释放该块
        }
    }

};


#endif //B_PLUS_TREE_FILE_OPERATOR_H