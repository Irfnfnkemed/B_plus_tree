#ifndef B_PLUS_TREE_EXCEPTIONS_H
#define B_PLUS_TREE_EXCEPTIONS_H

#include <string>

class exception {

public:
    exception() {}

    virtual std::string what() = 0;
};

class repeated_key : public exception {
    virtual std::string what() { return "The key is invalidly repeated!"; }
};

class repeated_key_and_value : public exception {
    virtual std::string what() { return "The key and value are invalidly repeated!"; }
};

class overlong_string : public exception {
    virtual std::string what() { return "The string is overlong!"; }
};

class repeated_ID : public exception {
    virtual std::string what() { return "The ID has been existed already!"; }
};

class none_exist_ID : public exception {
    virtual std::string what() { return "The ID isn't exist!"; }
};

class invalid_call : public exception {
    virtual std::string what() { return "The function is called invalidly!"; }
};

class unknown_error : public exception {
    virtual std::string what() { return "An unknown error occurred!"; }
};

#endif //B_PLUS_TREE_EXCEPTIONS_H
