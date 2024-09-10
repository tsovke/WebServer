#pragma once

#include "../log/log.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <functional>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

typedef std::function<void()> TimeoutCallback;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

struct TimerNode {
  int id;
  TimeStamp expires;
  TimeoutCallback cb;
  bool operator<(const TimerNode &t) { return expires < t.expires; }
  bool operator>(const TimerNode &t) { return expires > t.expires; }
};

class HeapTimer {
public:
  HeapTimer() { heap_.reserve(64); }
  ~HeapTimer() { clear(); }

  void adjust(int id, int newExpires);
  void add(int id, int timeOut, const TimeoutCallback &cb);
  void doWork(int id);
  void clear();
  void tick();
  void pop();
  int GetNextTick();

private:
  void del_(size_t i);
  void shiftup_(size_t i);
  bool shiftdown_(size_t i, size_t n);
  void SwapNode_(size_t i, size_t j);
  std::vector<TimerNode> heap_;
  std::unordered_map<int, size_t> ref_;
};
