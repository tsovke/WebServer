#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

template <typename Func> class ThreadPool {
public:
  ThreadPool(size_t numThreads = std::thread::hardware_concurrency())
      : stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
      workers.emplace_back([this] {
        while (true) {
          Func task;
          {
            std::lock_guard<std::mutex> lock(queue_mutex);
            conditon.wait(lock, [this] { return stop || !tasks.empty(); });
            if (stop && tasks.empty()) {
              return;
            }
            task = std::move(tasks.front());
            tasks.pop();
          }
          task();
        }
      });
    }
  }

  template <typename... Args>
  auto enqueue(Func &&func, Args &&...args)
      -> std::future<typename std::invoke_result_t<Func(Args...)>> {
    using return_type = typename std::invoke_result_t<Func(Args...)>;
    auto task = std::make_shared<std::packaged_task<return_type(Args...)>>(
        std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
    std::future<return_type> res = task->get_future();
    {
      std::lock_guard<std::mutex> lock(queue_mutex);
      // don't allow enqueueing after stoping the pool
      if (stop) {
        throw std::runtime_error("enqueue on stoped ThreadPool");
      }
      tasks.emplace(
          [task, args...]() { (*task)(std::forward<Args>(args)...); });
    }
    conditon.notify_one();
    return res;
  }

  ~ThreadPool(){
    {
      std::lock_guard<std::mutex> lock(queue_mutex);
      stop=true;
    }
    conditon.notify_all();
    for (std::thread& worker:workers ) {
      worker.join();
    }
  }

private:
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;

  std::mutex queue_mutex;
  std::condition_variable conditon;
  bool stop;
};
