#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <utility>

class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 4);
    ~ThreadPool();

    // 提交任务到线程池
    template<typename F, typename... Args>
    auto enqueue(F &&f, Args &&... args) -> std::future<typename std::invoke_result<F, Args...>::type>;

    // 禁止拷贝和赋值
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};

// 辅助类：存储函数和参数，避免 std::bind / std::apply
template<typename F, typename... Args>
struct TaskWrapper {
    F func;
    std::tuple<Args...> args;

    template<typename FwdF, typename... FwdArgs>
    TaskWrapper(FwdF &&f, FwdArgs &&... a)
        : func(std::forward<FwdF>(f)), args(std::forward<FwdArgs>(a)...) {}

    // 用 index_sequence 解包 tuple
    template<size_t... I>
    auto call(std::index_sequence<I...>) {
        return func(std::move(std::get<I>(args))...);
    }
};

template<typename F, typename... Args>
auto ThreadPool::enqueue(F &&f, Args &&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
    using returnType = typename std::invoke_result<F, Args...>::type;

    auto wrapper = std::make_shared<TaskWrapper<F, Args...>>(
        std::forward<F>(f), std::forward<Args>(args)...);

    auto task = std::make_shared<std::packaged_task<returnType()>>(
        [wrapper]() -> returnType {
            return wrapper->call(std::index_sequence_for<Args...>{});
        }
    );

    std::future<returnType> result = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (stop) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        tasks.emplace([task]() { (*task)(); });
    }
    condition.notify_one();
    return result;
}

#endif //THREADPOOL_H
