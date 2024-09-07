#include <condition_variable>
#include <mutex>

class MutexLock {
public:
  MutexLock() : mtx_(std::make_unique<std::mutex>()) {}
  void lock() { mtx_->lock(); }
  void unlock() { mtx_->unlock(); }

private:
  std::unique_ptr<std::mutex> mtx_;
};

class ConditionVariable {
public:
  ConditionVariable() : cv_(std::make_unique<std::condition_variable>()) {}
  void wait(std::unique_lock<std::mutex> &lock) { cv_->wait(lock); }
  void notify_one() { cv_->notify_one(); }
  void notify_all() { cv_->notify_all(); }

private:
  std::unique_ptr<std::condition_variable> cv_;
};

class Semaphore {
public:
  Semaphore(unsigned count = 0) : mtx_(), cond_(), count_(count) {}

  void wait() {
    std::unique_lock<std::mutex> lock(mtx_);
    while (count_ == 0) {
      cond_.wait(lock);
    }
    --count_;
  }

  void notify(unsigned cout = 1) {
    std::unique_lock<std::mutex> lock(mtx_);
    count_ += cout;
    cond_.notify_one();
  }
  unsigned getCount() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return count_;
  }

private:
  mutable std::mutex mtx_;
  std::condition_variable cond_;
  unsigned count_;
};
