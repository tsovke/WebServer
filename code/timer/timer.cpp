#include "timer.h"

void HeapTimer::del_(size_t i) {
  assert(i >= 0 && i < heap_.size());
  size_t n = heap_.size() - 1;
  assert(i <= n);
  if (i < heap_.size() - 1) {
    SwapNode_(i, heap_.size() - 1);
    if (!shiftdown_(i, n)) {
      shiftup_(i);
    }
  }
  ref_.erase(heap_.back().id);
  heap_.pop_back();
}

void HeapTimer::shiftup_(size_t i) {
  assert(i >= 0 && i < heap_.size());
  size_t parent = (i - 1) / 2;
  while (parent >= 0) {
    if (heap_[parent] > heap_[i]) {
      SwapNode_(i, parent);
      i = parent;
      parent = (i - 1) / 2;

    } else {
      break;
    }
  }
}

bool HeapTimer::shiftdown_(size_t i, size_t n) {

  assert(i >= 0 && i < heap_.size());
  assert(n >= 0 && n < heap_.size());
  size_t idx = i;
  size_t child = i * 2 + 1;
  while (child < n) {
    if (child + 1 < n && heap_[child + 1] < heap_[child]) {
      ++child;
    }
    if (heap_[child] < heap_[idx]) {
      SwapNode_(idx, child);
      idx = child;
      child = child * 2 + 1;
    }
    break;
  }
  return idx > i;
}

void HeapTimer::SwapNode_(size_t i, size_t j) {
  assert(i >= 0 && i < heap_.size());
  assert(j >= 0 && j < heap_.size());
  std::swap(heap_[i], heap_[j]);
  ref_[heap_[i].id] = i;
  ref_[heap_[j].id] = j;
}
void HeapTimer::adjust(int id, int newExpires) {
  assert(!heap_.empty() && ref_.count(id));
  heap_[ref_[id]].expires = Clock::now() + MS(newExpires);
  shiftdown_(ref_[id], heap_.size());
}

void HeapTimer::add(int id, int timeOut, const TimeoutCallback &cb) {
  assert(id >= 0);
  if (ref_.count(id)) {
    heap_[ref_[id]].expires = Clock::now() + MS(timeOut);
    heap_[ref_[id]].cb = cb;
  } else {
    size_t n = heap_.size();
    ref_[id] = n;
    heap_.push_back({id, Clock::now() + MS(timeOut), cb});
    shiftup_(n);
  }
}

void HeapTimer::doWork(int id) {
  if (heap_.empty() || ref_.count(id) == 0) {
    return;
  }
  size_t i = ref_[id];
  auto node = heap_[i];
  node.cb();
  del_(i);
}

void HeapTimer::clear() {
  ref_.clear();
  heap_.clear();
}

void HeapTimer::tick() {
  if (heap_.empty()) {
    return;
  }
  while (!heap_.empty()) {
    auto node = heap_.front();
    if (std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() >
        0) {
      break;
    }
    node.cb();
    pop();
  }
}

void HeapTimer::pop() {
  assert(!heap_.empty());
  del_(0);
}

int HeapTimer::GetNextTick() {
  tick();
  size_t res = -1;
  if (!heap_.empty()) {
    res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now())
              .count();
    if (res < 0) {
      res = 0;
    }
  }
  return (int)res;
}
