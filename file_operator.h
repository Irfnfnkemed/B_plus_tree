#ifndef B_PLUS_TREE_FILE_OPERATOR_H
#define B_PLUS_TREE_FILE_OPERATOR_H

#include <fstream>
#include <iostream>


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

template<class content, int max>
class cache {//实现缓冲区读写，缓存content类信息块
private:
    struct cache_node {
        size_t address;
        cache_node *next;
        cache_node *pre;
        content *to;

        cache_node(size_t address_ = 0, cache_node *next_ = nullptr,
                   cache_node *pre_ = nullptr, content *to_ = nullptr) {
            address = address_;
            next = next_;
            pre = pre_;
            if (next != nullptr) { next->pre = this; }
            if (pre != nullptr) { pre->next = this; }
            to = to_;
        }
    };

    int size;
    cache_node *head;
    cache_node *tail;
    hash_link<cache_node, 49999> random_access;

    void pop(std::fstream &file) {//缓存已满，将链表头部弹出，并更新对应文件
        --size;
        cache_node *p = head->next;
        head->next = p->next;
        p->next->pre = head;
        random_access.erase(p->address);//清除hash表对应内容
        file.seekg(p->address, std::ios::beg);
        file.write(reinterpret_cast<char *>(p->to), sizeof(content));//将缓存的内容存入文件中
        delete p->to;//删除缓存
        delete p;//删除节点
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
        now->pre->next = now->next;
        now->next->pre = now->pre;
        delete now->to;
        delete now;
        --size;
    }

public:

    //store_root为真，表示文件开头存储根节点位置;反之，不存储
    cache() {
        head = new cache_node;
        tail = new cache_node;
        head->next = tail;
        tail->pre = head;
        size = 0;
    }

    void clear(std::fstream &file) {
        while (head->next != tail) { pop(file); }
        delete head;
        delete tail;
    }

    ~cache() {}

    content *get(size_t address, std::fstream &file) {
        //访问address对应的文件内容
        //若缓存中已经存储了该地址对应信息，则返回对应内容，并将其提到link尾部
        //若缓存中未存储该地址对应信息，则文件中读取，插到link尾部，返回对应内容
        //若在插入缓存时，发现缓存已满，则弹出link头部的内容，并更新到文件中
        cache_node *p = random_access.find(address);
        if (p == nullptr) {
            if (size == max) { pop(file); }//若已满，弹出最前的一条信息
            p = new cache_node(address, tail, tail->pre, new content);
            file.seekg(address, std::ios::beg);
            file.read(reinterpret_cast<char *>(p->to), sizeof(content));
            random_access.insert(address, p);
            ++size;
        } else { adjust(p); }
        return p->to;
    }

    void free_memory(size_t address) {//释放(若存在)对应的缓存
        cache_node *p = random_access.find(address);
        if (p != nullptr) {//若缓存中有
            random_access.erase(address);//删除hash_link对应内容
            erase(p);//删除节点
        }
    }
};

template<class key_node, class info_node>
class files {
private:
    cache<key_node, 1000> cache_key;//key相关的缓存
    cache<info_node, 1000> cache_info;//info相关的缓存
    size_t key_root;//根的位置
    size_t free_head;//第一个空闲节点的位置
    std::fstream file;

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
            file.seekg(0, std::ios::end);
            get_addr(1);//创建空的第一个信息节点
        } else {
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char *>(&key_root), sizeof(size_t));
            file.read(reinterpret_cast<char *>(&free_head), sizeof(size_t));
        }
    }

    ~files() {
        file.seekg(0, std::ios::beg);
        file.write(reinterpret_cast<char *>(&key_root), sizeof(size_t));//更新根节点位置
        file.write(reinterpret_cast<char *>(&free_head), sizeof(size_t));//更新第一个空闲节点位置
        cache_key.clear(file);
        cache_info.clear(file);
        file.close();
    }

    inline key_node *get_key(size_t address) { return cache_key.get(address, file); }

    inline info_node *get_info(size_t address) { return cache_info.get(address, file); }

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
        file.seekg(address, std::ios::beg);
        file.write(reinterpret_cast<char *>(&free_head), sizeof(size_t));//更新空闲节点形成的链表
        free_head = address;
        if (!cat) { cache_key.free_memory(address); }
        else { cache_info.free_memory(address); }
    }

    inline size_t get_root_addr() { return key_root; }

    inline void update_root_addr(size_t addr) { key_root = addr; }

};

class base_of_snapshot_father {
public:
    base_of_snapshot_father() {}

    ~base_of_snapshot_father() {}

    virtual void erase_addr(size_t addr) = 0;

    virtual void change_addr(size_t old_addr, size_t new_addr) = 0;

    virtual void change_father(size_t addr, size_t new_father) = 0;

    virtual void add_addr(size_t addr, size_t fa = 0, int ref = 1) = 0;

    virtual void change_reference(size_t addr, int change_ref) = 0;
};

#endif //B_PLUS_TREE_FILE_OPERATOR_H