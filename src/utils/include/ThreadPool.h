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

/**
 * 线程池类
 * 提供固定数量工作线程的任务队列，支持提交任务并获取 future 结果
 * @author fgsq
 * @version 1.0
 */
class ThreadPool {
public:
    /**
     * 构造函数
     * @param threadCount 工作线程数量，默认 4
     */
    explicit ThreadPool(size_t threadCount = 4);

    /** 析构函数，等待所有任务完成后停止所有线程 */
    ~ThreadPool();

    /**
     * 提交任务到线程池
     * @tparam F 可调用对象类型
     * @tparam Args 参数类型
     * @param f 可调用对象
     * @param args 参数
     * @return std::future 可用于获取任务返回值
     */
    template<typename F, typename... Args>
    auto enqueue(F &&f, Args &&... args) -> std::future<typename std::invoke_result<F, Args...>::type>;

    /** 禁止拷贝 */
    ThreadPool(const ThreadPool &) = delete;
    /** 禁止赋值 */
    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};

/**
 * 任务包装辅助类
 * 存储函数和参数，避免 std::bind / std::apply
 */
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
