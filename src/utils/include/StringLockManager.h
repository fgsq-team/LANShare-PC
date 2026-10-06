//
// Created by fgsqme on 2024/5/4.
//

#ifndef LANSHARE_STRINGLOCKMANAGER_H
#define LANSHARE_STRINGLOCKMANAGER_H

#include <iostream>
#include <string>
#include <map>
#include <mutex>

/**
 * 字符串锁管理器
 * 通过字符串键获取对应的互斥锁，用于细粒度的多线程同步
 * @author fgsq
 * @version 1.0
 */
class StringLockManager {
public:
    /**
     * 获取指定键的互斥锁
     * 若键不存在则自动创建
     * @param key 锁的唯一标识
     * @return 对应的互斥锁引用
     */
    static std::mutex &getStringLock(const std::string &key);
};


#endif //LANSHARE_STRINGLOCKMANAGER_H
