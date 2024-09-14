#pragma once

#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

#include "../epoller/epoller.h"
#include "../http/httpconn.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../timer/timer.h"

class WebServer {
public:
  WebServer(int port, int trigMode, int timeoutMS, int sqlPort,
            const char *sqlUser, const char *sqlPwd, const char *dbName,
            int connPoolNum, int threadNum, bool openLog, int logLevel,
            int logQueSize);
  ~WebServer();
  void Start();

private:
  bool InitSocket_();
  void InitEventMode_(int trigMode);
  void AddClient_(int fd, sockaddr_in addr);
  void DealListen_();
  void DealWrite_(HttpConn *client);
  void DealRead_(HttpConn *client);
  void SendError_(int fd, const char *info);
  void ExtentTime_(HttpConn *client);
  void CloseConn_(HttpConn *client);
  void OnRead_(HttpConn *client);
  void OnWrite_(HttpConn *client);
  void Onprocess(HttpConn *client);

  static int SetFdNonblock(int fd);

  static const int MAX_FD = 65535;

  int port_;
  bool openLinger_;
  int timeoutMS_;
  bool isClose_;
  int listenFd_;
  char *srcDir_;
  uint32_t listenEvent_;
  uint32_t connEvent_;
  std::unique_ptr<HeapTimer> timer_;
  std::unique_ptr<ThreadPool> threadpool_;
  std::unique_ptr<Epoller> epoller_;
  std::unordered_map<int, HttpConn> users_;
};
