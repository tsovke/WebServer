#include <memory>
#include <mutex>

class MutexLock {
  MutexLock() : mtx_(std::make_unique<std::mutex>()) {}
  void lock() { mtx_->lock(); }
  void unlock() { mtx_->unlock(); }

private:
  std::unique_ptr<std::mutex> mtx_;
};
