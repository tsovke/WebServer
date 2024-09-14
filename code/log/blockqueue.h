#pragma once

#include <cassert>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <sys/time.h>
#include <type_traits>

template <typename T> class BlockQueue {
public:
  explicit BlockQueue(size_t maxsize = 1000);
  ~BlockQueue();
  bool empty();
  bool full();
  void push_back(const T &item);
  void push_front(const T &item);
  bool pop(T &item); // 弹出任务放入item
  bool pop(T &item, int timeout);
  void clear();
  T front();
  T back();
  size_t capacity();
  size_t size();
  void Flush();
  void Close();

private:
  std::deque<T> deq_;
  std::mutex mtx_;
  bool isClose_;
  size_t capacity_;
  std::condition_variable condConsumer_;
  std::condition_variable condProducer_;
};

template <typename T> void BlockQueue<T>::Close() {
  clear();
  isClose_ = true;
  condConsumer_.notify_all();
  condProducer_.notify_all();
}

template <typename T> void BlockQueue<T>::clear() {
  std::lock_guard<std::mutex> lock(mtx_);
  deq_.clear();
}

template <typename T> bool BlockQueue<T>::empty() {
  std::lock_guard<std::mutex> lock(mtx_);
  return deq_.empty();
}
template <typename T> bool BlockQueue<T>::full() {
  std::lock_guard<std::mutex> lock(mtx_);
  return deq_.size() == capacity_;
}

// 生产者
template <typename T> void BlockQueue<T>::push_back(const T &item) {
  std::unique_lock<std::mutex> lock(mtx_);
  while (deq_.size() >= capacity_) {
    condProducer_.wait(lock);
  }
  deq_.push_back(item);
  condConsumer_.notify_one();
}
// 生产者
template <typename T> void BlockQueue<T>::push_front(const T &item) {
  std::unique_lock<std::mutex> lock(mtx_);
  while (deq_.size() >= capacity_) {
    condProducer_.wait(lock);
  }
  deq_.push_front(item);
  condConsumer_.notify_one();
}

template <typename T> bool BlockQueue<T>::pop(T &item) {
  std::unique_lock<std::mutex> lock(mtx_);
  while (deq_.empty()) {
    condProducer_.wait(lock);
  }
  item = deq_.front();
  deq_.pop_front();
  condProducer_.notify_one();
  return true;
}

template <typename T> bool BlockQueue<T>::pop(T &item, int timeout) {
  std::unique_lock<std::mutex> lock(mtx_);
  while (deq_.empty()) {
    if (condConsumer_.wait_for(lock, std::chrono::seconds(timeout)) ==
        std::cv_status::timeout)
      return false;
    if (isClose_)
      return false;
  }
  item = deq_.front();
  deq_.pop_front();
  condProducer_.notify_one();
  return true;
}

template <typename T> T BlockQueue<T>::front() {
  std::lock_guard<std::mutex> lock(mtx_);
  return deq_.front();
}

template <typename T> T BlockQueue<T>::back() {
  std::lock_guard<std::mutex> lock(mtx_);
  return deq_.back();
}

template <typename T> size_t BlockQueue<T>::capacity() {
  std::lock_guard<std::mutex> lock(mtx_);
  return capacity_;
}

template <typename T> size_t BlockQueue<T>::size() {
  std::lock_guard<std::mutex> lock(mtx_);
  return deq_.size();
}

template <typename T> void BlockQueue<T>::Flush() {
  condConsumer_.notify_one();
}
