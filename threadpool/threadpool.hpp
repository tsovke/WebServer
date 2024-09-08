#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>
#include <iostream>


template <typename Func> class ThreadPool {
public:
  ThreadPool(size_t numThreads = std::thread::hardware_concurrency())
      : stop(false) {
    try {
      for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] {
          while (true) {
            Func task;
            {
              std::unique_lock<std::mutex> lock(queue_mutex);
              condition.wait(lock, [this] { return stop || !tasks.empty(); });
              if (stop && tasks.empty()) {
                return;
              }
              task = std::move(tasks.front());
              tasks.pop();
            }
            try{task();}catch(const std::exception& e){
              std::cerr<<"Exception caught: "<<e.what()<<std::endl;
            }catch(...){
              std::cerr<<"Unknown exception caught"<<std::endl;
            }
            
          }
        });
      }
    } catch (...) {
      // Ensure all threads are joined before rethrowing
      for (auto &worker : workers) {
        if (worker.joinable()) {
          worker.join();
        }
      }
      throw;
    }
  }

  template <typename... Args>
  auto enqueue(Func &&func, Args &&...args)
      -> std::future<decltype(func(args...))> {
    using return_type = decltype(func(args...));

    std::lock_guard<std::mutex> lock(queue_mutex);
    // don't allow enqueueing after stoping the pool
    try {
      if (stop) {
        throw std::runtime_error("enqueue on stoped ThreadPool");
      }
      auto task = std::make_shared<std::packaged_task<return_type(Args...)>>(
          std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
      std::future<return_type> res = task->get_future();

      tasks.emplace(
          [task, args...]() { (*task)(std::forward<Args>(args)...); });

      condition.notify_one();
      return res;
    } catch (...) {
      throw;
    }
  }

  ~ThreadPool() noexcept {
    try {
      {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop = true;
      }
      condition.notify_all();
      for (std::thread &worker : workers) {
        worker.join();
      }
    } catch (...) {
      throw;
    }
  }

private:
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;

  std::mutex queue_mutex;
  std::condition_variable condition;
  bool stop;
};
