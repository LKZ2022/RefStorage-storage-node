//Copyright (c) 2026 Liu Kaizhi
//Licensed under the Apache License, Version 2.0.


#include "../include/ThreadPool.hpp"

namespace ref_storage::utils {

    ThreadPool::ThreadPool(size_t threads) {

        try {
            for (size_t i = 0; i < threads; i++) {
                workers.emplace_back([this]() {
                    while (true) {
                        std::packaged_task<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(queue_mutex);
                            condition.wait(lock, [this] { return stop || !tasks.empty(); });
                            if (stop && tasks.empty())
                                return;
                            task = std::move(tasks.front());
                            tasks.pop();
                        }
                        task();  // 执行任务
                    }
                });
            }
        }catch (...) {
            {
                std::unique_lock<std::mutex> lock(this->queue_mutex);
                stop = true;
            }
            condition.notify_all();
            for (std::thread& worker : workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
            throw;
        }

    }

    ThreadPool::~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers) {
            worker.join();
        }
    }
}
