#include "threadpool.hpp"
#include <iostream>
#include <chrono>

// 示例任务函数
int add(int x, int y) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 模拟耗时操作
    return x + y;
}

// 另一个示例任务函数
void print_message(const std::string& msg) {
    std::cout << msg << std::endl;
}

int main() {
    try {
        // 创建一个线程池，使用默认的线程数
        ThreadPool<std::function<void()>> pool;

        // 提交并执行带有返回值的任务
        std::future<int> result_future = pool.enqueue(std::bind(add, 2, 3));
        std::cout << "The sum is: " << result_future.get() << std::endl;

        // 提交无返回值的任务
        pool.enqueue(std::bind(print_message, "Hello from thread pool!"));

        // 异常处理示例（尝试在已停止的线程池中添加任务）
        pool.stop(); // 假设添加了一个stop方法来停止线程池
        try {
            pool.enqueue(std::bind(print_message, "This should not be printed"));
        } catch (const std::runtime_error& e) {
            std::cout << e.what() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Caught exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}