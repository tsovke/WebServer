#pragma once

#include "../log/log.h"
#include <cassert>
#include <cstdint>
#include <mutex>
#include <mysql/mysql.h>
#include <queue>
#include <semaphore.h>
#include <string>
#include <thread>

class SqlConnPool {
public:
  static SqlConnPool *Instance();

  MYSQL *GetConn();
  void FreeConn(MYSQL *conn);
  int GetFreeConnCount();

  void Init(const char *host, uint16_t port, const char *user, const char *pwd,
            const char *dbName, int connSize);
  void ClosePool();

private:
  SqlConnPool() = default;
  ~SqlConnPool() { ClosePool(); }

  int MAX_CONN_;

  std::queue<MYSQL *> connQue_;
  std::mutex mtx_;
  sem_t semId_;
};

class SqlConnRAII {
public:
  SqlConnRAII(MYSQL **sql, SqlConnPool *connpool) {
    assert(connpool);
    *sql = connpool->GetConn();
    sql_ = *sql;
    connpool_ = connpool;
  }

  ~SqlConnRAII() {
    if (sql_) {
      connpool_->FreeConn(sql_);
    }
  }

private:
  MYSQL *sql_;
  SqlConnPool *connpool_;
};
