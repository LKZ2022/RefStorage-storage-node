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
        auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;
    };


}
