#pragma once

#include <cassert>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

class ThreadPool {
public:
  ThreadPool() = default;
  ThreadPool(ThreadPool &&) = default;
  explicit ThreadPool(int threadCount = 16) : pool_(std::make_shared<Pool>()) {
    assert(threadCount > 0);
    for (int i = 0; i < threadCount; ++i) {
      std::thread([this]() {
        std::unique_lock<std::mutex> lock(pool_->mtx_);
        while (true) {
          if (!pool_->tasks.empty()) {
            auto task = std::move(pool_->tasks.front());
            pool_->tasks.pop();
            lock.unlock();
            task();
            lock.lock();
          } else if (pool_->isClosed) {
            break;
          } else {
            pool_->cond_.wait(lock);
          }
        }
      }).detach();
    }
  }

  ~ThreadPool() {
    if (pool_) {
      std::unique_lock<std::mutex> lock(pool_->mtx_);
      pool_->isClosed = true;
    }
    pool_->cond_.notify_all();
  }

  template <typename T> void AddTask(T &&task) {
    std::unique_lock<std::mutex> lock(pool_->mtx_);
    pool_->tasks.emplace(std::forward<T>(task));
    pool_->cond_.notify_one();
  }

private:
  struct Pool {
    std::mutex mtx_;
    std::condition_variable cond_;
    bool isClosed;
    std::queue<std::function<void()>> tasks;
  };
  std::shared_ptr<Pool> pool_;
};
