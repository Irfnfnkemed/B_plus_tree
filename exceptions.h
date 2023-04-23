#ifndef B_PLUS_TREE_EXCEPTIONS_H
#define B_PLUS_TREE_EXCEPTIONS_H

#include <string>

class exception {

public:
    exception() {}

    virtual std::string what() = 0;
};

class repeated_key : public exception {
    virtual std::string what() { return "The key is invalidly repeated."; }
};

class repeated_key_and_value : public exception {
    virtual std::string what() { return "The key and value are invalidly repeated."; }
};

#endif //B_PLUS_TREE_EXCEPTIONS_H
