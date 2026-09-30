//
// Created by fgsqme on 2024/5/4.
//

#include "StringLockManager.h"

std::map<std::string, std::mutex> stringLocks;
std::mutex mutex_;

std::mutex &StringLockManager::getStringLock(const std::string &key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = stringLocks.find(key);
    if (it == stringLocks.end()) {
        // 如果找不到对应的锁，则创建一个新的并插入到map中
        return stringLocks[key];
    }
    return it->second;
}