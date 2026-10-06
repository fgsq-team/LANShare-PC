#include "ThreadPool.h"

/**
 * 构造函数，启动指定数量的工作线程
 */
ThreadPool::ThreadPool(size_t threadCount) : stop(false) {
    for (size_t i = 0; i < threadCount; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queueMutex);
                    this->condition.wait(lock, [this] {
                        return this->stop || !this->tasks.empty();
                    });
                    if (this->stop && this->tasks.empty()) {
                        return;
                    }
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
            }
        });
    }
}

/**
 * 析构函数，设置停止标志并等待所有工作线程结束
 */
ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread &worker: workers) {
        worker.join();
    }
}
