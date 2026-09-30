//
// Created by fgsqme on 2023/2/2.
//

#ifndef LANSHARE_WIN_LEXCEPTION_H
#define LANSHARE_WIN_LEXCEPTION_H

#include <string>

class LException {
private:
    std::string message;

public:
    LException(std::string str) : message{str} {}
    std::string what() const { return message; }
};


#endif //LANSHARE_WIN_LEXCEPTION_H
