//
// Created by fgsqme on 2024/5/4.
//

#include "StringLockManager.h"

std::map<std::string, std::mutex> stringLocks;
std::mutex mutex_;

/**
 * 获取指定键的互斥锁
 * 若键不存在则自动创建
 */
std::mutex &StringLockManager::getStringLock(const std::string &key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = stringLocks.find(key);
    if (it == stringLocks.end()) {
        // 如果找不到对应的锁，则创建一个新的并插入到map中
        return stringLocks[key];
    }
    return it->second;
}