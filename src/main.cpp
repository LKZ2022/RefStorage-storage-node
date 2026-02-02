#include <iostream>
#include <../src/utils/include/ThreadPool.hpp>

std::mutex mtx;

void testPrint(int msg) {
    mtx.lock();
    std::cout << "Thread ID: " << std::this_thread::get_id() <<" executing operation output to console, output value: " << msg << std::endl;
    mtx.unlock();
}


int main() {
    ref_storage::utils::ThreadPool  thread_pool(10);

    for (int i = 0; i < 10; i++) {
        thread_pool.enqueue([i]() {testPrint(i);});
    }

    return 0;

}