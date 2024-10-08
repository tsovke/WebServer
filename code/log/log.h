#pragma once

#include "../buffer/buffer.h"
#include "blockqueue.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <thread>
#include <utility>

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

#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::Instance();\
        if (log->IsOpen() && log->GetLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);

// 四个宏定义，主要用于不同类型的日志输出，也是外部使用日志的接口
// ...表示可变参数，__VA_ARGS__就是将...的值复制到这里
// 前面加上##的作用是：当可变参数的个数为0时，这里的##可以把把前面多余的","去掉,否则会编译出错。
#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);    
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);
