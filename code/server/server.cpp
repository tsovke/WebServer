#include "server.h"

WebServer::WebServer(int port, int trigMode, int timeoutMS, int sqlPort,
                     const char *sqlUser, const char *sqlPwd,
                     const char *dbName, int connPoolNum, int threadNum,
                     bool openLog, int logLevel, int logQueSize)
    : port_(port), timeoutMS_(timeoutMS), isClose_(false),
      timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)),
      epoller_(new Epoller()) {
  if (openLog) {
    Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
    if (isClose_) {
      LOG_ERROR("======== Server init error!i ========");

    } else {
      LOG_INFO("==============================");
      LOG_INFO("======== Server init! ========");
      LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
               (listenEvent_ & EPOLLET ? "ET" : "LT"),
               (connEvent_ & EPOLLET ? "ET" : "LT"));
      LOG_INFO("LogSys level:: %d", logLevel);
      LOG_INFO("srcDir: %s", HttpConn::srcDir);
      LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum,
               threadNum);
      LOG_INFO("==============================");
    }
  }
  srcDir_ = getcwd(nullptr, 256);
  assert(srcDir_);
  strcat(srcDir_, "/resources");
  HttpConn::userCount = 0;
  HttpConn::srcDir = srcDir_;

  SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName,
                                connPoolNum);
  InitEventMode_(trigMode);
  if (!InitSocket_()) {
    isClose_ = true;
  }
}

WebServer::~WebServer() {
  close(listenFd_);
  isClose_ = true;
  free(srcDir_);
  SqlConnPool::Instance()->ClosePool();
}

void WebServer::InitEventMode_(int trigMode) {
  listenEvent_ = EPOLLRDHUP;
  connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
  switch (trigMode) {
  case 0:
    break;
  case 1:
    connEvent_ |= EPOLLET;
    break;
  case 2:
    listenEvent_ |= EPOLLET;
    break;
  case 3:
    listenEvent_ |= EPOLLET;
    connEvent_ |= EPOLLET;
    break;
  default:
    listenEvent_ |= EPOLLET;
    connEvent_ |= EPOLLET;
    break;
  }
  HttpConn::isET = (connEvent_ & EPOLLET);
}

void WebServer::Start() {
  int timeMS = -1;
  if (!isClose_) {
    LOG_INFO("======== Server start! ========");
  }
  while (!isClose_) {
    if (timeoutMS_ > 0) {
      timeMS = timer_->GetNextTick();
    }
    int eventCnt = epoller_->Wait(timeMS);
    for (int i = 0; i < eventCnt; ++i) {
      int fd = epoller_->GetEventFd(i);
      uint32_t events = epoller_->GetEventFd(i);
      if (fd == listenFd_) {
        DealListen_();
      } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
        assert(users_.count(fd) > 0);
        CloseConn_(&users_[fd]);
      } else if (events & EPOLLIN) {
        assert(users_.count(fd) > 0);
        DealRead_(&users_[fd]);
      } else if (events & EPOLLOUT) {
        assert(users_.count(fd) > 0);
        DealWrite_(&users_[fd]);
      } else {
        LOG_ERROR("Unexpected event");
      }
    }
  }
}

void WebServer::SendError_(int fd, const char *info) {
  assert(fd > 0);
  if (send(fd, info, strlen(info), 0))
    LOG_WARN("send error to client:[%d] error!", fd);
  close(fd);
}

void WebServer::CloseConn_(HttpConn *client) {
  assert(client);
  LOG_INFO("Client:[%d] quit.", client->GetFd());
  epoller_->DelFd(client->GetFd());
  client->Close();
}

void WebServer::AddClient_(int fd, sockaddr_in addr) {
  assert(fd > 0);
  users_[fd].init(fd, addr);
  if (timeoutMS_ > 0) {
    timer_->add(fd, timeoutMS_,
                std::bind(&WebServer::CloseConn_, this, &users_[fd]));
  }
  epoller_->AddFd(fd, EPOLLIN | connEvent_);
  SetFdNonblock(fd);
  LOG_INFO("Client:[%d] in!", users_[fd].GetFd());
}

void WebServer::DealListen_() {
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  do {
    int fd =
        accept(listenFd_, reinterpret_cast<struct sockaddr *>(&addr), &len);
    if (fd <= 0) {
      return;
    } else if (HttpConn::userCount >= MAX_FD) {
      SendError_(fd, "Server busy!");
      LOG_WARN("Client is full!");
      return;
    }
    AddClient_(fd, addr);
  } while (listenEvent_ & EPOLLET);
}
void WebServer::DealRead_(HttpConn *client) {
  assert(client);
  ExtentTime_(client);
  threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}
void WebServer::DealWrite_(HttpConn *client) {
  assert(client);
  ExtentTime_(client);
  threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

void WebServer::ExtentTime_(HttpConn *client) {
  assert(client);
  if (timeoutMS_ > 0) {
    timer_->adjust(client->GetFd(), timeoutMS_);
  }
}

void WebServer::OnRead_(HttpConn *client) {
  assert(client);
  int ret = -1, readErrno = 0;
  ret = client->read(&readErrno);
  if (ret <= 0 && readErrno != EAGAIN) {
    CloseConn_(client);
    return;
  }
  Onprocess(client);
}

void WebServer::OnWrite_(HttpConn *client) {
  assert(client);
  int ret = -1, writeErrno = 0;
  ret = client->write(&writeErrno);
  if (client->ToWriteBytes() == 0) {
    if (client->IsKeepAlive()) {
      epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
      return;
    }

  } else if (ret < 0) {
    if (writeErrno == EAGAIN) {
      epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
      return;
    }
  }
  CloseConn_(client);
}

void WebServer::Onprocess(HttpConn *client) {
  if (client->process()) {
    epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
  } else {
    epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
  }
}

bool WebServer::InitSocket_() {
  int ret;
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port_);

  listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listenFd_ < 0) {
    LOG_ERROR("Create socket error!");
    return false;
  }

  int reuse = 1;
  ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuse,
                   sizeof(reuse));
  if (ret == -1) {
    LOG_ERROR("set socket setsockopt error!");
    close(listenFd_);
    return false;
  }

  ret =
      bind(listenFd_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
  if (ret < 0) {
    LOG_ERROR("bind error!");
    close(listenFd_);
    return false;
  }

  ret = listen(listenFd_, 5);
  if (ret < 0) {
    LOG_ERROR("listen error!");
    close(listenFd_);
    return false;
  }

  ret = epoller_->AddFd(listenFd_, listenEvent_ | EPOLLIN);
  if (ret == 0) {
    LOG_ERROR("addFd listen error!");
    close(listenFd_);
    return false;
  }
  SetFdNonblock(listenFd_);
  LOG_INFO("server port:%d", port_);
  return true;
}

int WebServer::SetFdNonblock(int fd) {
  assert(fd > 0);
  return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}
