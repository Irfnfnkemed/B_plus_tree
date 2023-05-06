#ifndef B_PLUS_TREE_B_PLUS_TREE_H
#define B_PLUS_TREE_B_PLUS_TREE_H

#include "file_operator.h"
#include "exceptions.h"

//存储Key—Information对，具有允许、不允许Key重复两种模式，但不论如何，不允许Key、Information都重复
//key_node_size是key节点空间，info_node_size是info节点空间
//Key、Information需要重载operator< 、需有默认构造函数
//info_operator类用于find函数和modify函数，需要有find(Information)，not_find()，modify(Information,const &Information)函数
template<class Key, class Information, int node_size, class info_operator>
class B_plus_tree {
private:
    //max_key_number为key_node中存储的key的最多数目(1-based)
    //max_info_number为info_node中存储的信息的最多数目(1-based)
    //node_key_surplus、node_info_surplus分别表示用于补齐空间的char[]的大小
    static const int max_key_number = (node_size - 13) / (sizeof(Key) + 8) - 1 +
                                      ((node_size - 13) / (sizeof(Key) + 8)) % 2;
    static const int max_info_number = (node_size - 4) / (sizeof(Key) + sizeof(Information)) -
                                       ((node_size - 4) / (sizeof(Key) + sizeof(Information))) % 2;
    static const int node_key_surplus = node_size - 13 - max_key_number * (sizeof(Key) + 8);
    static const int node_info_surplus = node_size - 4 - max_info_number * (sizeof(Key) + sizeof(Information));

    enum category {
        KEY, INFO
    };

#pragma pack(push, 1)

    struct key_node {
        int number = 0;//已有Key的条数,1-based
        bool is_leaf = false;//是否为叶节点
        Key key[max_key_number];//第i个值表示第(i+1)棵子树中最小的Key,0-based
        size_t address[max_key_number + 1] = {2 * sizeof(size_t) + node_size};//第i个值表第i棵子树位置,0-based
        char completion[node_key_surplus];//补齐节点大小

        void add(const Key &key_, size_t address_, int pos) {//插入下标为pos的子树
            for (int i = number; i >= pos; --i) {
                if (i > 0) { key[i] = key[i - 1]; }
                address[i + 1] = address[i];
            }
            if (pos > 0) { key[pos - 1] = key_; }
            else { key[0] = key_; }
            address[pos] = address_;
            ++number;
        }

        void remove(int pos) {//删除下标为pos处的子树
            for (int i = pos; i < number; ++i) {
                if (i > 0) { key[i - 1] = key[i]; }
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
    bool is_key_repeated = true;//为true，允许key重复；反之，则不可。但是不论怎样，不允许Key、Information都重复

    //判断是否要继续进入对应块中进行操作
    inline bool judge_key(key_node *key_tmp, int i, const Key &key) {
        if (key_tmp->number == 0) { return true; }//此时，对应只有一个信息块情况
        if (i < key_tmp->number) { return !(key_tmp->key[i] < key); }
        else { return true; }
    }

    //判断是否是可插入位置(mark为true，表这是树的最后一个节点)
    inline bool judge_insert(info_node *info_tmp, const Key &key,
                             const Information &info, int i, bool mark) {
        if (info_tmp->number == 0) { return true; }//空结点，直接插入
        if (info_tmp->number == i) { return mark; }//最后一个位置，为了防止value失去顺序，插到下一个块中(除了最后一个节点外)
        if (info_tmp->key[i] < key) { return false; }
        if (is_key_repeated) {
            if (key < info_tmp->key[i]) { return true; }
            if (info_tmp->info[i] < info) { return false; }
            if (info < info_tmp->info[i]) { return true; }
            throw repeated_key_and_value();//key-info对与已存储信息重复，抛出错误
        } else { throw repeated_key(); }//key与已存储key重复，抛出错误
    }

    //判断是否可以向左借块
    inline bool judge_borrow_left(key_node *key_tmp, int i) {
        if (i == 0) { return false; }
        if (key_tmp->is_leaf) {
            info_node *info_tmp = Files.get_info(key_tmp->address[i - 1]);
            return info_tmp->number > max_info_number / 2;
        } else {
            key_node *key_tmp_left = Files.get_key(key_tmp->address[i - 1]);
            return key_tmp_left->number > max_key_number / 2;
        }
    }

    //判断是否可以向右借块
    inline bool judge_borrow_right(key_node *key_tmp, int i) {
        if (i == key_tmp->number) { return false; }
        if (key_tmp->is_leaf) {
            info_node *info_tmp = Files.get_info(key_tmp->address[i + 1]);
            return info_tmp->number > max_info_number / 2;
        } else {
            key_node *key_tmp_left = Files.get_key(key_tmp->address[i + 1]);
            return key_tmp_left->number > max_key_number / 2;
        }
    }

    //若未成功删除，返回false，表继续删除位置
    //反之，返回true
    //flag为true，表示需继续向上裂块
    //mark为true，表示需要修改索引
    //key_new为新的索引
    bool erase(const Key &key, const Information &info,
               size_t address, bool &flag, bool &mark, Key &key_new) {
        key_node *key_tmp = Files.get_key(address);
        if (key_tmp->is_leaf) {//叶节点，到info对应文件里查找
            info_node *info_tmp;
            for (int i = 0; i <= key_tmp->number; ++i) {
                if (judge_key(key_tmp, i, key)) {
                    info_tmp = Files.get_info(key_tmp->address[i]);
                    for (int j = 0; j < info_tmp->number; ++j) {
                        if (info_tmp->key[j] < key) { continue; }
                        else if (key < info_tmp->key[j]) { return false; }//无对应信息，删除失败
                        else if (info_tmp->info[j] < info) { continue; }
                        else if (info < info_tmp->info[j]) { return false; }//无对应信息，删除失败
                        else {//找到信息，删除
                            info_tmp->remove(j);
                            if (j == 0) {
                                if (i >= 1) {//删除最前面，更新节点索引，不需向上调整索引
                                    key_tmp->key[i - 1] = info_tmp->key[0];
                                    mark = false;
                                } else {//向上返回需调整的索引
                                    mark = true;
                                    key_new = info_tmp->key[0];
                                }
                            } else { mark = false; }
                            if (info_tmp->number < max_info_number / 2) {
                                flag = adjust_erase(key_tmp, i);//进行借块或并块调整，并更新flag
                            } else { flag = false; }
                            return true;
                        }
                    }
                }
            }
            return false;//未删除
        } else {//非叶节点，递归删除
            for (int i = 0; i <= key_tmp->number; ++i) {
                if (judge_key(key_tmp, i, key)) {
                    if (erase(key, info, key_tmp->address[i], flag, mark, key_new)) {//已经成功删除
                        if (mark && i >= 1) {//更新节点索引
                            key_tmp->key[i - 1] = key_new;
                            mark = false;
                        } //向上返回需调整的索引
                        if (flag) { flag = adjust_erase(key_tmp, i); }//进行借块或并块调整，并更新flag
                        return true;//成功删除
                    }
                    key_tmp = Files.get_key(address);//防止key_tmp失效
                }
            }
            return false;//没有删除
        }
    }

    //返回true，表示需继续向上调整
    bool adjust_erase(key_node *key_tmp, int i) {//对key_tmp的第i个儿子(0-based)进行借块或并块
        if (key_tmp->is_leaf) {//叶节点，调整信息节点
            info_node *info_now = Files.get_info(key_tmp->address[i]);
            info_node *info_borrow;
            if (key_tmp->number == 0) { return false; }//表只有一个根节点，不需调整
            if (judge_borrow_left(key_tmp, i)) {
                info_borrow = Files.get_info(key_tmp->address[i - 1]);
                info_now->add(info_borrow->key[info_borrow->number - 1],
                              info_borrow->info[info_borrow->number - 1], 0);//借入
                info_borrow->remove(info_borrow->number - 1);//移除
                key_tmp->key[i - 1] = info_now->key[0];//更新索引
            } else if (judge_borrow_right(key_tmp, i)) {
                info_borrow = Files.get_info(key_tmp->address[i + 1]);
                info_now->add(info_borrow->key[0], info_borrow->info[0], info_now->number);//借入
                info_borrow->remove(0);//移除
                key_tmp->key[i] = info_now->key[info_now->number - 1];//更新索引
            } else { //将后面的一块合并到当前块上
                if (i == key_tmp->number) {//若为最后，为了方便，向前移动一位
                    --i;
                    info_borrow = info_now;
                    info_now = Files.get_info(key_tmp->address[i]);
                } else { info_borrow = Files.get_info(key_tmp->address[i + 1]); }
                for (int j = 0; j < info_borrow->number; ++j) {//向前并，移动
                    info_now->key[j + info_now->number] = info_borrow->key[j];
                    info_now->info[j + info_now->number] = info_borrow->info[j];
                }
                info_now->number += info_borrow->number;
                Files.free_addr(key_tmp->address[i + 1], INFO);//释放空间
                key_tmp->remove(i + 1);
            }
            return key_tmp->number < max_key_number / 2;
        } else {
            key_node *key_now = Files.get_key(key_tmp->address[i]);
            key_node *key_borrow;
            if (judge_borrow_left(key_tmp, i)) {
                key_borrow = Files.get_key(key_tmp->address[i - 1]);
                key_now->add(key_tmp->key[i - 1], key_borrow->address[key_borrow->number], 0);//借入
                key_tmp->key[i - 1] = key_borrow->key[key_borrow->number - 1];//更新索引
                key_borrow->remove(key_borrow->number);//移除
            } else if (judge_borrow_right(key_tmp, i)) {
                key_borrow = Files.get_key(key_tmp->address[i + 1]);
                key_now->add(key_tmp->key[i], key_borrow->address[0], key_now->number + 1);//借入
                key_tmp->key[i] = key_borrow->key[0];//更新索引
                key_borrow->remove(0);//移除
            } else {//将后面的一块合并到当前块上
                if (i == key_tmp->number) {//若为最后，为了方便，向前移动一位
                    --i;
                    key_borrow = key_now;
                    key_now = Files.get_key(key_tmp->address[i]);
                } else { key_borrow = Files.get_key(key_tmp->address[i + 1]); }
                for (int j = 0; j < key_borrow->number; ++j) {//向前并，移动
                    key_now->key[j + key_now->number + 1] = key_borrow->key[j];
                    key_now->address[j + key_now->number + 1] = key_borrow->address[j];
                }
                key_now->key[key_now->number] = key_tmp->key[i];//更新索引
                key_now->address[key_now->number + key_borrow->number + 1] = key_borrow->address[key_borrow->number];
                key_now->number += (key_borrow->number + 1);
                Files.free_addr(key_tmp->address[i + 1], KEY);//释放空间
                key_tmp->remove(i + 1);
            }
            return key_tmp->number < max_key_number / 2;
        }
    }


    //若未成功插入，返回false，表继续寻找合适位置插入
    //反之，返回true
    //flag为true，表示需继续向上裂块
    //mark为true，表示这是树上最右一条路径上的节点
    bool insert(const Key &key, const Information &info,
                size_t addr, bool &flag, bool mark = true) {
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
                    key_tmp = Files.get_key(addr);//防止key_tmp失效
                }
            }
            return false;//未成功插入
        }
    }

    //返回true，表示需继续向上调整
    bool adjust_insert(key_node *key_tmp, int i) {//对key_tmp的第i个儿子(0-based)进行裂块
        if (key_tmp->is_leaf) {
            //先更新信息节点
            size_t new_info_addr = Files.get_addr(INFO);//得到新空间
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
            size_t new_key_addr = Files.get_addr(KEY);//得到新空间
            key_node *key_old = Files.get_key(key_tmp->address[i]);
            key_node *key_new = Files.get_key(new_key_addr);
            key_new->is_leaf = key_old->is_leaf;//更新叶节点标记
            for (int j = 0; j < max_key_number / 2; ++j) {//转移
                key_new->key[j] = key_old->key[j + max_key_number / 2 + 1];
                key_new->address[j] = key_old->address[j + max_key_number / 2 + 1];
            }
            key_new->address[max_key_number / 2] = key_old->address[max_key_number];
            key_old->number = max_key_number / 2;
            key_new->number = max_key_number / 2;
            //再将新的儿子插到key_tmp中
            key_tmp->add(key_old->key[max_key_number / 2], new_key_addr, i + 1);
            return (key_tmp->number == max_key_number);
        }
    }

    //mark为0，表示查找模式；mark为1，表示修改模式
    //若目标key值只可能出现在当前子树上，返回false，表结束查找
    //反之，返回true，表继续查找
    bool find_and_modify(const Key &key, size_t addr, bool &flag, int mark, const Information &info = Information()) {
        key_node *key_tmp = Files.get_key(addr);
        if (key_tmp->is_leaf) {//叶节点，到info对应文件里查找
            info_node *info_tmp;
            for (int i = 0; i <= key_tmp->number; ++i) {
                if (judge_key(key_tmp, i, key)) {
                    info_tmp = Files.get_info(key_tmp->address[i]);
                    for (int j = 0; j < info_tmp->number; ++j) {
                        if (info_tmp->key[j] < key) { continue; }
                        else if (key < info_tmp->key[j]) { return false; }//结束
                        else {
                            if (!mark) { Info_operator.find(info_tmp->info[j]); }
                            else { Info_operator.modify(info_tmp->info[j], info); }
                            flag = true;
                        }
                    }
                }
            }
            return true;//没有在中间结束，表面后面可能还有符合要求的信息，继续查找
        } else {//非叶节点，递归查找
            for (int i = 0; i <= key_tmp->number; ++i) {
                if (judge_key(key_tmp, i, key)) {
                    if (!find_and_modify(key, key_tmp->address[i], flag, mark, info)) { return false; }//已经结束
                    key_tmp = Files.get_key(addr);//防止key_tmp失效
                }
            }
            return true;//没有在中间结束，表面后面可能还有符合要求的信息，继续查找
        }
    }

public:

    info_operator Info_operator;


    B_plus_tree(char file_name[], bool flag = true) : Files(file_name) {
        is_key_repeated = flag;
        //处理key的根节点信息
        key_node *key_root = Files.get_key(Files.get_root_addr());
        if (key_root->number == 0) { key_root->is_leaf = true; }
    }

    ~B_plus_tree() {}


    //找到所有键值为key的信息，并按value从小到大依次进行操作
    //若未找到，进行相应操作
    //包裹函数，兼具判断是否找到的功能
    void find(const Key &key) {
        bool flag = false;
        find_and_modify(key, Files.get_root_addr(), flag, 0);
        if (!flag) { Info_operator.not_find(); }
    }

    //找到键值为key的信息，并修改对应的value
    //若未找到，进行相应操作
    //仅适用于键值不可重复的情况
    //包裹函数，兼具判断是否找到的功能
    void modify(const Key &key, const Information &info) {
        if (is_key_repeated) { throw invalid_call(); }
        bool flag = false;
        find_and_modify(key, Files.get_root_addr(), flag, 1, info);
        if (!flag) { Info_operator.not_find(); }
    }

    //插入指定的key-info对
    //若key或key-info对(视具体模式而定)与已存储信息重复，抛出相应的错误
    //包裹函数，兼具分裂根节点的功能
    void insert(const Key &key, const Information &info) {
        bool flag;
        insert(key, info, Files.get_root_addr(), flag);
        if (flag) {//需要分裂根节点
            size_t new_key_addr_root = Files.get_addr(KEY);//得到新空间
            size_t new_key_addr_one = Files.get_addr(KEY);//得到新空间
            size_t new_key_addr_two = Files.get_addr(KEY);//得到新空间
            key_node *key_root = Files.get_key(Files.get_root_addr());
            key_node *key_new_root = Files.get_key(new_key_addr_root);
            key_node *key_new_one = Files.get_key(new_key_addr_one);
            key_node *key_new_two = Files.get_key(new_key_addr_two);
            for (int i = 0; i < max_key_number / 2; ++i) {//更新第一个节点
                key_new_one->key[i] = key_root->key[i];
                key_new_one->address[i] = key_root->address[i];
            }
            key_new_one->is_leaf = key_root->is_leaf;
            key_new_one->address[max_key_number / 2] = key_root->address[max_key_number / 2];
            key_new_one->number = max_key_number / 2;
            for (int i = 0; i < max_key_number / 2; ++i) {//更新第二个节点
                key_new_two->key[i] = key_root->key[i + max_key_number / 2 + 1];
                key_new_two->address[i] = key_root->address[i + max_key_number / 2 + 1];
            }
            key_new_two->is_leaf = key_root->is_leaf;
            key_new_two->address[max_key_number / 2] = key_root->address[max_key_number];
            key_new_two->number = max_key_number / 2;
            //更新新的根节点
            key_new_root->is_leaf = false;
            key_new_root->key[0] = key_root->key[max_key_number / 2];
            key_new_root->address[0] = new_key_addr_one;
            key_new_root->address[1] = new_key_addr_two;
            key_new_root->number = 1;
            //处理原根节点
            Files.free_addr(Files.get_root_addr(), KEY);//释放空间
            Files.update_root_addr(new_key_addr_root);//更新根节点位置
        }
    }

    //删除指定的key-info对
    //若指定key-info不存在，不修改内容
    //包裹函数，兼具合并根节点的功能
    void erase(const Key &key, const Information &info) {
        Key key_tmp;
        bool flag, mark;
        erase(key, info, Files.get_root_addr(), flag, mark, key_tmp);
        key_node *key_root = Files.get_key(Files.get_root_addr());
        if (!key_root->is_leaf && key_root->number == 0) {//需要减小树高
            size_t key_addr_new_root = key_root->address[0];
            Files.free_addr(Files.get_root_addr(), KEY);//释放空间
            Files.update_root_addr(key_addr_new_root);//更新根节点位置
        }
    }
};

#endif //B_PLUS_TREE_B_PLUS_TREE_H
