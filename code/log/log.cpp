#include "log.h"

Log::Log() {
  fp_ = nullptr;
  deque_ = nullptr;
  writeThread_ = nullptr;
  lineCount_ = 0;
  toDay_ = 0;
  isAsync_ = false;
}

Log::~Log() {
  while (!deque_->empty()) {
    deque_->Flush();
  }
  deque_->Close();
  writeThread_->join();
  if (fp_) {
    std::lock_guard<std::mutex> lock(mtx_);
    flush();
    fclose(fp_);
  }
}

void Log::flush() {
  if (isAsync_)
    deque_->Flush();
  fflush(fp_);
}

// 懒汉模式，局部静态变量法
Log *Log::Instance() {
  static Log log;
  return &log;
}

void Log::FlushLogThread() { Log::Instance()->AsyncWrite_(); }

void Log::AsyncWrite_() {
  std::string str = "";
  while (deque_->pop(str)) {
    std::lock_guard<std::mutex> lock(mtx_);
    fputs(str.c_str(), fp_);
  }
}

void Log::init(int level, const char *path, const char *suffix,
               int maxQueCapacity) {
  isOpen_ = true;
  level_ = level;
  path_ = path;
  suffix_ = suffix;
  if (maxQueCapacity) {
    isAsync_ = true;
    if (!deque_) {
      std::unique_ptr<BlockQueue<std::string>> newQue(
          new BlockQueue<std::string>);
      deque_ = std::move(newQue);
      std::unique_ptr<std::thread> newThread(new std::thread(FlushLogThread));
      writeThread_ = std::move(newThread);
    }
  } else {
    isAsync_ = false;
  }

  lineCount_ = 0;
  time_t timer = time(nullptr);
  struct tm *sysTime = localtime(&timer);
  path_ = path;
  suffix_ = suffix;
  char fileName[LOG_NAME_LEN] = {0};
  snprintf(fileName, LOG_NAME_LEN - 1, "%s%04d_%02d_%02d%s", path_,
           sysTime->tm_year + 1900, sysTime->tm_mon + 1, sysTime->tm_mday,
           suffix_);
  toDay_ = sysTime->tm_mday;

  {
    std::lock_guard<std::mutex> lock(mtx_);
    buff_.RetrieveAll();
    if (fp_) {
      flush();
      fclose(fp_);
    }
    fp_ = fopen(fileName, "a"); // 附加写
    if (fp_ == nullptr) {
      mkdir(path_, 0777);
      fp_ = fopen(fileName, "a"); // 附加写
    }
    assert(fp_);
  }
}

void Log::write(int level, const char *format, ...) {
  struct timeval now = {0, 0};
  gettimeofday(&now, nullptr);
  time_t tSec = now.tv_sec;
  struct tm *sysTime = localtime(&tSec);

  va_list vaList;

  if (toDay_ != sysTime->tm_mday ||
      (lineCount_ && (lineCount_ % MAX_LINES == 0))) {
    std::unique_lock<std::mutex> lock(mtx_);
    lock.unlock();
    char newFile[LOG_NAME_LEN] = {0};
    char tail[36] = {0};
    snprintf(tail, 36, "%04d_%02d_%02d", sysTime->tm_year + 1900,
             sysTime->tm_mon + 1, sysTime->tm_mday);
    if (toDay_ != sysTime->tm_mday) {
      snprintf(newFile, LOG_NAME_LEN - 72, "%s%s%s", path_, tail, suffix_);
      toDay_ = sysTime->tm_mday;
      lineCount_ = 0;
    } else {
      snprintf(newFile, LOG_NAME_LEN - 72, "%s%s-%d%s", path_, tail,
               (lineCount_ / MAX_LINES), suffix_);
    }

    lock.lock();
    flush();
    fclose(fp_);
    fp_ = fopen(newFile, "a");
    assert(fp_);
  }

  // 在buffer生成一条日志
  {
    std::unique_lock<std::mutex> lock(mtx_);
    ++lineCount_;
    int n = snprintf(
        buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
        sysTime->tm_year + 1900, sysTime->tm_mon + 1, sysTime->tm_mday,
        sysTime->tm_hour, sysTime->tm_min, sysTime->tm_sec, now.tv_usec);
    buff_.HasWritten(n);
    AppendLogLevelTitle_(level);
    va_start(vaList, format);
    int m =
        vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
    va_end(vaList);

    buff_.HasWritten(m);
    buff_.Append("\n\0", 2);

    if (isAsync_ && deque_ && !deque_->full()) {
      deque_->push_back(buff_.RetrieveAllToStr());
    } else {
      fputs(buff_.Peek(), fp_);
    }
    buff_.RetrieveAll();
  }
}

void Log::AppendLogLevelTitle_(int level) {
  switch (level) {
  case 0:
    buff_.Append("[debug]: ", 9);
    break;
  case 1:
    buff_.Append("[info] : ", 9);
    break;
  case 2:
    buff_.Append("[warn] : ", 9);
    break;
  case 3:
    buff_.Append("[error]: ", 9);
    break;
  default:
    buff_.Append("[info] : ", 9);
    break;
  }
}

int Log::GetLevel() {
  std::lock_guard<std::mutex> lock(mtx_);
  return level_;
}

void Log::SetLevel(int level) {
  std::lock_guard<std::mutex> lock(mtx_);
  level_ = level;
}
