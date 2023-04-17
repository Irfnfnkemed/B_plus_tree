#ifndef B_PLUS_TREE_B_PLUS_TREE_H
#define B_PLUS_TREE_B_PLUS_TREE_H

#include "file_operator.h"

template<class Key, class Information,
        int max_key_number, int max_info_number,
        int node_key_surplus, int node_info_surplus>
class B_plus_tree {
private:
#pragma pack(push, 1)

    struct key_node {
        int number = 0;//已有Key的条数,1-based
        bool is_leaf = false;//是否为叶节点
        Key key[max_key_number];//第i个值表示第(i+1)棵子树中最小的Key,0-based
        size_t address[max_key_number + 1] = {0};//第i个值表第i棵子树位置,0-based
        char completion[node_key_surplus];//补齐节点大小

        void add(const Key &key_, size_t address_, int pos) {//插入下标为pos的子树
            for (int i = number; i >= pos; --i) {
                key[i] = key[i - 1];
                address[i + 1] = address[i];
            }
            key[pos - 1] = key_;
            address[pos] = address_;
            ++number;
        }

        void remove(int pos) {//删除下标为pos处的子树
            for (int i = pos; i < number; ++i) {
                key[i - 1] = key[i];
                address[i] = address[i + 1];
            }
            --number;
        }
    };

    struct info_node {
        int number = 0;//已有Key的条数,1-based
        Key key[max_info_number];//第i个值表示第i个Key,0-based
        Information info[max_info_number];//第i个值表第i个Key对应信息,0-based
        char completion[node_info_surplus];//补齐剩余空间

        info_node(int number_ = 0) { number = number_; }

        void add(const Key &key_, const Information &information_, int pos) {//插入到下标为pos处
            for (int i = number - 1; i >= pos; --i) {
                key[i + 1] = key[i];
                info[i + 1] = info[i];
            }
            key[pos] = key_;
            info[pos] = information_;
            ++number;
        }

        void remove(int pos) {//删除下标为pos处的信息
            for (int i = pos; i <= number - 2; ++i) {
                key[i] = key[i + 1];
                info[i] = info[i + 1];
            }
            --number;
        }
    };

#pragma pack(pop)

    files<key_node, info_node> Files;

    inline bool judge_key(key_node *key_tmp, int i, const Key &key) {//判断是否要继续进入对应块中进行操作
        if (key_tmp->number == 0) { return true; }//此时，对应只有一个信息块情况
        if (i < key_tmp->number) { return !(key_tmp->key[i] < key); }
        else { return true; }
    }

    inline bool judge_insert(info_node *info_tmp, const Key &key,
                             const Information &info, int i, bool mark) {
        //判断是否是可插入位置(mark为true，表这是树的最后一个节点)
        if (info_tmp->number == 0) { return true; }//空结点，直接插入
        if (info_tmp->number == i) { return mark; }//最后一个位置，为了防止value失去顺序，插到下一个块中(除了最后一个节点外)
        if (info_tmp->key[i] < key) { return false; }
        if (key < info_tmp->key[i]) { return true; }
        if (info_tmp->info[i] < info) { return false; }
        if (info < info_tmp->info[i]) { return true; }
        throw 1;
    }

public:
    B_plus_tree(char file_key_name[], char file_info_name[],
                char file_pool_key_name[], char file_pool_info_name[]) :
            Files(file_key_name, file_info_name, file_pool_key_name, file_pool_info_name) {
        key_node *key_root = Files.get_key(0);
        if (key_root->number == 0) { key_root->is_leaf = true; }
    }

    ~B_plus_tree() {}

    //找到所有键值为key的信息，并按value从小到大输出value值
    bool find(const Key &key) { find(key, 0); }//包裹函数

    //若目标key值只可能出现在当前子树上，返回false，表结束查找
    //反之，返回true，表继续查找
    bool find(const Key &key, size_t addr) {
        key_node *key_tmp = Files.get_key(addr);
        if (key_tmp->is_leaf) {//叶节点，到info对应文件里查找
            info_node *info_tmp;
            for (int i = 0; i <= key_tmp->number; ++i) {
                if (judge_key(key_tmp, i, key)) {
                    info_tmp = Files.get_info(key_tmp->address[i]);
                    for (int j = 0; j < info_tmp->number; ++j) {
                        if (info_tmp->key[j] < key) { continue; }
                        else if (key < info_tmp->key[j]) { return false; }//结束
                        else { info_tmp->info[j].print(); }
                    }
                }
            }
            return true;//没有在中间结束，表面后面可能还有符合要求的信息，继续查找
        } else {//非叶节点，递归查找
            for (int i = 0; i <= key_tmp->number; ++i) {
                if (judge_key(key_tmp, i, key)) {
                    if (!find(key, key_tmp->address[i])) { return false; }//已经结束
                }
            }
            return true;//没有在中间结束，表面后面可能还有符合要求的信息，继续查找
        }
    }

    //包裹函数，兼具分裂根节点的功能
    void insert(const Key &key, const Information &info) {
        bool flag;
        insert(key, info, 0, flag);
        if (flag) {//需要分裂根节点
            size_t new_key_addr_one = Files.get_addr_key();//得到新空间
            size_t new_key_addr_two = Files.get_addr_key();//得到新空间
            key_node *key_root = Files.get_key(0);
            key_node *key_new_one = Files.get_key(new_key_addr_one);
            key_node *key_new_two = Files.get_key(new_key_addr_two);
            for (int i = 0; i < max_key_number / 2; ++i) {//更新第一个节点
                key_new_one->key[i] = key_root->key[i];
                key_new_one->address[i] = key_root->address[i];
            }
            key_new_one->is_leaf = key_root->is_leaf;
            key_new_one->address[max_key_number / 2] = key_root->address[max_key_number / 2];
            key_new_one->number = max_key_number / 2;
            for (int i = 0; i < max_key_number / 2 - 1; ++i) {//更新第二个节点
                key_new_two->key[i] = key_root->key[i + max_key_number / 2 + 1];
                key_new_two->address[i] = key_root->address[i + max_key_number / 2 + 1];
            }
            key_new_two->is_leaf = key_root->is_leaf;
            key_new_two->address[max_key_number / 2 - 1] = key_root->address[max_key_number];
            key_new_two->number = max_key_number / 2 - 1;
            //更新新的根节点
            key_root->is_leaf = false;
            key_root->key[0] = key_root->key[max_key_number / 2];
            key_root->address[0] = new_key_addr_one;
            key_root->address[1] = new_key_addr_two;
            key_root->number = 1;
        }
    }

    //若未成功插入，返回false，表继续寻找合适位置插入
    //反之，返回true
    //flag为true，表示需继续向上裂块
    //mark为true，表示这是树上最右一条路径上的节点
    bool insert(const Key &key, const Information &info, size_t addr, bool &flag, bool mark = true) {
        key_node *key_tmp = Files.get_key(addr);
        if (key_tmp->is_leaf) {
            info_node *info_tmp;
            for (int i = 0; i <= key_tmp->number; ++i) {
                if (judge_key(key_tmp, i, key)) {
                    info_tmp = Files.get_info(key_tmp->address[i]);
                    for (int j = 0; j <= info_tmp->number; ++j) {
                        if (judge_insert(info_tmp, key, info, j, mark && i == key_tmp->number)) {
                            info_tmp->add(key, info, j);
                            if (i >= 1 && j == 0) { key_tmp->key[i - 1] = key; }//插在最前面，更新节点索引
                            if (info_tmp->number == max_info_number) {
                                flag = adjust_insert(key_tmp, i);//进行裂块调整，并更新flag
                            } else { flag = false; }
                            return true;//成功插入
                        }
                    }
                }
            }
            return false;//未插入。返回false
        } else {//非叶节点，递归插入
            for (int i = 0; i <= key_tmp->number; ++i) {
                if (judge_key(key_tmp, i, key)) {
                    if (insert(key, info, key_tmp->address[i], flag, mark && i == key_tmp->number)) {//已经成功插入
                        if (i >= 1 && key < key_tmp->key[i - 1]) { key_tmp->key[i - 1] = key; }//插在最前面，更新节点索引
                        if (flag) { flag = adjust_insert(key_tmp, i); }//进行裂块调整，并更新flag
                        return true;//成功插入
                    }
                }
            }
            return false;//未成功插入
        }
    }

    bool adjust_insert(key_node *key_tmp, int i) {//对key_tmp的第i个儿子(0-based)进行裂块
        if (key_tmp->is_leaf) {
            //先更新信息节点
            size_t new_info_addr = Files.get_addr_info();//得到新空间
            info_node *info_old = Files.get_info(key_tmp->address[i]);
            info_node *info_new = Files.get_info(new_info_addr);
            for (int j = 0; j < max_info_number / 2; ++j) {//转移
                info_new->key[j] = info_old->key[j + max_info_number / 2];
                info_new->info[j] = info_old->info[j + max_info_number / 2];
            }
            info_old->number = info_new->number = max_info_number / 2;
            //再将新的儿子插到key_tmp中
            key_tmp->add(info_new->key[0], new_info_addr, i + 1);
            return (key_tmp->number == max_key_number);
        } else {
            size_t new_key_addr = Files.get_addr_key();//得到新空间
            key_node *key_old = Files.get_key(key_tmp->address[i]);
            key_node *key_new = Files.get_key(new_key_addr);
            key_new->is_leaf = key_old->is_leaf;//更新叶节点标记
            for (int j = 0; j < max_key_number / 2 - 1; ++j) {//转移
                key_new->key[j] = key_old->key[j + max_key_number / 2 + 1];
                key_new->address[j] = key_old->address[j + max_key_number / 2 + 1];
            }
            key_new->address[max_key_number / 2 - 1] = key_old->address[max_key_number];
            key_old->number = max_key_number / 2;
            key_new->number = max_key_number / 2 - 1;
            //再将新的儿子插到key_tmp中
            key_tmp->add(key_old->key[max_key_number / 2], new_key_addr, i + 1);
            return (key_tmp->number == max_key_number);
        }
    }


};

#endif //B_PLUS_TREE_B_PLUS_TREE_H
