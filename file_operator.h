#ifndef B_PLUS_TREE_FILE_OPERATOR_H
#define B_PLUS_TREE_FILE_OPERATOR_H

#include <fstream>
#include <iostream>

inline bool open_file(std::fstream &file, char file_name[]) {
    file.open(file_name, std::ios::in);
    if (file.is_open()) {
        file.close();
        file.open(file_name, std::ios::in | std::ios::out);
        return true;
    } else {
        file.open(file_name, std::ios::out);
        file.close();
        file.open(file_name, std::ios::in | std::ios::out);
        return false;
    }
}

class pool {//实现文件空间的回收与分配
    static const size_t size_node_pool = 4096;
    static const int max_pool_number = size_node_pool / sizeof(size_t);
private:
#pragma pack(push, 1)
    struct memory_record {//除了只有一个结构体的情况，其余情况储存的条数范围：[1/4满,满)
        size_t address[max_pool_number];//现在file_info中空余的位置
    };
#pragma pack(pop)
    //file_pool文件的开头依次存储：对应的file中的节点个数，file_pool中储存的地址条数
    std::fstream file_pool;//用于分配、回收对应文件中的空间
    memory_record mem_pool;//缓存file_pool中结构体的信息
    size_t pool_number;//file_pool中储存的地址条数(1-based)
    size_t mem_pool_number;//mem_pool中储存的信息条数(1-based)
public:
    pool(char file_pool_name[]) {
        if (open_file(file_pool, file_pool_name)) {
            file_pool.seekg(0, std::ios::beg);
            file_pool.read(reinterpret_cast<char *>(&pool_number), sizeof(size_t));
            if (pool_number >= max_pool_number / 2) {//mem_poo存储半满
                file_pool.seekg(pool_number * sizeof(size_t) - size_node_pool / 2, std::ios::cur);
                mem_pool_number = max_pool_number / 2;
            } else { mem_pool_number = pool_number; }
            file_pool.read(reinterpret_cast<char *>(&mem_pool), size_node_pool / 2);
            file_pool.seekg(-(int) size_node_pool / 2, std::ios::cur);//将文件指针指向对应位置
        } else {
            pool_number = mem_pool_number = 0;
            file_pool.seekg(0, std::ios::beg);
            file_pool.write(reinterpret_cast<char *>(&pool_number), sizeof(size_t));
        }
    }

    ~pool() {
        file_pool.write(reinterpret_cast<char *>(&mem_pool), size_node_pool);//将缓存中的内容存入
        file_pool.seekg(0, std::ios::beg);
        file_pool.write(reinterpret_cast<char *>(&pool_number), sizeof(size_t));//更新信息条数
        file_pool.close();
    }

    bool empty() const { return !pool_number; }

    //给出pool中记录的相应文件中的空间，并返回对应起始地址
    //若相应pool的缓存的信息条数小于满数量的1/4，在pool中再取1/2满数量，放入缓存
    size_t get_pool_memory() {
        if (pool_number < max_pool_number || mem_pool_number > 0) {
            --mem_pool_number;
        } else {
            mem_pool_number += (max_pool_number / 2 - 1);
            file_pool.seekg(-(int) size_node_pool / 2, std::ios::cur);
            file_pool.read(reinterpret_cast<char *>(&mem_pool), size_node_pool);
            file_pool.seekg(-size_node_pool, std::ios::cur);//文件指针指向此块的开头
        }
        --pool_number;
        return mem_pool.address[mem_pool_number];
    }

    //将相应的空间记录放入pool中
    //若相应pool的缓存的信息条数等于满数量，将缓存中满数量的1/2写入pool文件，并更新缓存
    void free_memory(size_t free_address) {
        mem_pool.address[mem_pool_number] = free_address;
        ++pool_number;
        ++mem_pool_number;
        if (mem_pool_number == max_pool_number) {
            file_pool.write(reinterpret_cast<char *>(&mem_pool), size_node_pool / 2);//文件指针指向此块的开头
            for (int i = 0; i < max_pool_number / 2; ++i) {
                mem_pool.address[i] = mem_pool.address[i + max_pool_number / 2];
            }//更新缓存
            mem_pool_number -= max_pool_number / 2;
        }
    }
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
    std::fstream file;
    cache_node *head;
    cache_node *tail;
    hash_link<cache_node, 49999> random_access;
    pool content_pool;
    size_t key_root;


    void pop() {//缓存已满，将链表头部弹出，并更新对应文件
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
    cache(char file_name[], char file_pool_name[], bool store_root = false) : content_pool(file_pool_name) {
        if (!open_file(file, file_name)) {//创建初始空根节点
            if (store_root) {
                key_root = sizeof(size_t);
                file.seekg(0, std::ios::beg);
                file.write(reinterpret_cast<char *>(&key_root), sizeof(size_t));
            } else { file.seekg(0, std::ios::beg); }
            get_memory();
        } else {
            if (store_root) {
                file.seekg(0, std::ios::beg);
                file.read(reinterpret_cast<char *>(&key_root), sizeof(size_t));
            }
        }
        head = new cache_node;
        tail = new cache_node;
        head->next = tail;
        tail->pre = head;
        size = 0;
    }

    ~cache() {
        if (key_root != 0) {//更新根节点位置
            file.seekg(0, std::ios::beg);
            file.write(reinterpret_cast<char *>(&key_root), sizeof(size_t));
        }
        while (head->next != tail) { pop(); }
        delete head;
        delete tail;
        file.close();
    }

    content *get(size_t address) {
        //访问address对应的文件内容
        //若缓存中已经存储了该地址对应信息，则返回对应内容，并将其提到link尾部
        //若缓存中未存储该地址对应信息，则文件中读取，插到link尾部，返回对应内容
        //若在插入缓存时，发现缓存已满，则弹出link头部的内容，并更新到文件中
        cache_node *p = random_access.find(address);
        if (p == nullptr) {
            if (size == max) { pop(); }//若已满，弹出最前的一条信息
            p = new cache_node(address, tail, tail->pre, new content);
            file.seekg(address, std::ios::beg);
            file.read(reinterpret_cast<char *>(p->to), sizeof(content));
            random_access.insert(address, p);
            ++size;
        } else { adjust(p); }
        return p->to;
    }

    size_t get_memory() {
        //给出一块可行空间的地址(分配空间)
        //若相应pool不空，那么给出相应pool中记录的相应文件中的空间，并返回对应起始地址，同时随之更新pool
        //若相应pool已经为空，那么直接在相应文件开新的空间，并返回对应起始地址
        if (content_pool.empty()) {//开辟一块新的空间
            file.seekg(0, std::ios::end);
            content tmp;
            file.write(reinterpret_cast<char *>(&tmp), sizeof(content));//生成空白区域
            size_t a = file.tellg();
            size_t b = sizeof(content);
            size_t c = a - b;
            return c;
        } else { return content_pool.get_pool_memory(); }
    }

    void free_memory(size_t address) {//将对应空间释放入pool中(释放空间),同时释放(若存在)对应的缓存
        content_pool.free_memory(address);
        cache_node *p = random_access.find(address);
        if (p != nullptr) {//若缓存中有
            random_access.erase(address);//删除hash_link对应内容
            erase(p);//删除节点
        }
    }

    inline size_t get_root_addr() { return key_root; }

    inline void update_root_addr(size_t addr) { key_root = addr; }

};

template<class key_node, class info_node>
class files {
private:
    cache<key_node, 100000> cache_key;//key相关的缓存
    cache<info_node, 100000> cache_info;//info相关的缓存
public:
    files(char file_key_name[], char file_info_name[],
          char file_pool_key_name[], char file_pool_info_name[]) ://初始化各个部分
            cache_key(file_key_name, file_pool_key_name, true),
            cache_info(file_info_name, file_pool_info_name, false) {}

    ~files() {}

    inline size_t get_root_addr() { return cache_key.get_root_addr(); }

    inline void update_root_addr(size_t addr) { return cache_key.update_root_addr(addr); }

    key_node *get_key(size_t address) { return cache_key.get(address); }

    info_node *get_info(size_t address) { return cache_info.get(address); }

    size_t get_addr_key() { return cache_key.get_memory(); }

    size_t get_addr_info() { return cache_info.get_memory(); }

    void free_addr_key(size_t address) { cache_key.free_memory(address); }

    void free_addr_info(size_t address) { cache_info.free_memory(address); }

};

#endif //B_PLUS_TREE_FILE_OPERATOR_H
