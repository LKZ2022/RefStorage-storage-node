//Copyright (c) 2026 Liu Kaizhi
//Licensed under the Apache License, Version 2.0.

#pragma once
#include <condition_variable>
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <future>
#include <mutex>

namespace ref_storage::utils {

    class ThreadPool {
    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()> > tasks;

        std::mutex queue_mutex;
        std::condition_variable condition;
        bool stop = false;

    public:
        explicit ThreadPool(size_t threads);
        ~ThreadPool();

        // Submit Task.
        template <class F, class... Args>
        auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {

            using return_type = typename std::invoke_result<F, Args...>::type;

            auto task = std::make_shared< std::packaged_task<return_type()> >(
                [func = std::forward<F>(f), args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                    return std::apply(func, std::move(args));
                }
            );

            std::future<return_type> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(queue_mutex);

                if (stop) {
                    throw std::runtime_error("enqueue on stopped ThreadPool");
                }
                tasks.emplace([task]() {
                    (*task)();
                });
            }

            condition.notify_one();
            return res;
        }
    };


}
