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
        std::queue< std::packaged_task<void()> > tasks;

        std::mutex queue_mutex;
        std::condition_variable condition;
        bool stop = false;

    public:
        explicit ThreadPool(size_t threads);
        ~ThreadPool();

        // Submit Task.
        template <class F, class... Args>
        auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
            using return_type = std::invoke_result_t<F, Args...>;

            // 创建 promise 和 future
            auto promise = std::make_shared<std::promise<return_type>>();
            auto future = promise->get_future();

            // NOLINT(cert-err58-cpp)
            auto task = [promise, f = std::forward<F>(f), args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                try {
                    if constexpr (std::is_void_v<return_type>) {
                        std::apply(f, std::move(args));
                        promise->set_value();
                    } else {
                        promise->set_value(std::apply(f, std::move(args)));
                    }
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            };

            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                if (stop) {
                    throw std::runtime_error("enqueue on stopped ThreadPool");
                }
                tasks.emplace(std::move(task));  // tasks 类型为 std::queue<std::function<void()>>
            }
            condition.notify_one();
            return future;
        }
    };


}
