# B_plus_tree

这是一个B+树模板类。该B+树类实现了快照功能，并且能够生成缓存、进行空间回收。

该B+树类存储`Key—Information`对，具有允许、不允许Key重复两种模式(可通过模板参数指定)，但不论如何，不允许Key、Information都重复。模板参数可以指定块的大小。同时，模板类还要求上传三个类：
`Key`,`Information`,`info_operator`。`Key`、`Information`类需要重载`operator<` 、需有默认构造函数。`info_operator`类用于`find`函数和`modify`函数，需要有`find(Information)`,`not_find()`,`modify(Information&)`函数。

具体接口如下：
```cpp
    //构造函数
    B_plus_snapshot_tree(char file_name[], char file_ID_name[],
                         char file_fa_name[], bool flag = true);
    
    //析构函数
    ~B_plus_snapshot_tree();

    //创建快照
    void create_snapshot(char SnapshotID[]);

    //删除快照
    void erase_snapshot(char SnapshotID[]);

    //恢复快照
    void restore_snapshot(char SnapshotID[]);

    //查找
    void find(const Key &key);

    //修改
    void modify(const Key &key);

    //插入
    void insert(const Key &key, const Information &info);

    //删除
    void erase(const Key &key, const Information &info);
```