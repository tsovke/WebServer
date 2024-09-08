#pragma once

#include "../bufer/bufer.h"
#include "blockqueue.h"
#include <cstdarg>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <thread>

class Log {
public:
  void init(int level, const char *path = "./log", const char *suffix = ".log",
            int maxQueueCapacity = 1024);
  static Log *Instance();
  static void FlushLogThread(); // 异步写日志，调用AsyncWrite_

  void write(int level, const char *format, ...);
  void flush();

  int GetLevel();
  void SetLevel(int level);
  bool IsOpen() { return isOpen_; }

private:
  Log();
  void AppendLogLevelTitle_(int level);
  virtual ~Log();
  void AsyncWrite_();

private:
  static const int LOG_PATH_LEN = 256;
  static const int LOG_NAME_LEN = 256;
  static const int MAX_LINES = 50000; // 最大日志条数

  const char *path_;
  const char *suffix_;

  int lineCount_; // 日志行数记录
  int toDay_;     // 按当天日期区分文件

  bool isOpen_;

  Buffer buff_;  // 日志输出的内容，缓冲区
  int level_;    // 日志等级
  bool isAsync_; // 是否开启异步日志

  FILE *fp_; // 打开log的文件指针
  std::unique_ptr<BlockQueue<std::string>> deque_;
  std::unique_ptr<std::thread> writeThread_;
  std::mutex mtx_;
};
