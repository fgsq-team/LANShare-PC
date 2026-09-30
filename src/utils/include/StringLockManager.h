//
// Created by fgsqme on 2024/5/4.
//

#ifndef LANSHARE_STRINGLOCKMANAGER_H
#define LANSHARE_STRINGLOCKMANAGER_H

#include <iostream>
#include <string>
#include <map>
#include <mutex>

class StringLockManager {
public:
    static std::mutex &getStringLock(const std::string &key);
};


#endif //LANSHARE_STRINGLOCKMANAGER_H
