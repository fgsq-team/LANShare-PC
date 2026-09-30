//
// Created by user on 12/3/24.
//

#ifndef CLEANUPTOOL_H
#define CLEANUPTOOL_H
#include <iostream>
#include <functional>

class CleanupTool {
public:
    // 构造函数接受一个可调用对象，如函数指针、lambda 或 std::function
    explicit CleanupTool(std::function<void()> func)
        : cleanupFunc(std::move(func)) {}

    // 禁止拷贝构造和赋值
    CleanupTool(const CleanupTool&) = delete;
    CleanupTool& operator=(const CleanupTool&) = delete;

    // 允许移动构造和赋值
    CleanupTool(CleanupTool&&) noexcept = default;
    CleanupTool& operator=(CleanupTool&&) noexcept = default;

    // 析构函数中调用传入的函数
    ~CleanupTool() {
        if (cleanupFunc) {
            cleanupFunc();
        }
    }

private:
    std::function<void()> cleanupFunc;
};

#endif //CLEANUPTOOL_H
