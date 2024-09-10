#pragma once

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <mysql/mysql.h>
#include <regex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

class HttpRequest {
public:
  enum PARSE_STATE {
    REQUEST_LINE,
    HEADERS,
    BODY,
    FINISH,
  };

  HttpRequest() { Init(); }
  ~HttpRequest() = default;

  void Init();
  bool parse(Buffer &buff);
  std::string path() const;
  std::string &path();
  std::string method() const;
  std::string version() const;
  std::string GetPost(const std::string &key) const;
  std::string GetPost(const char *key) const;
  bool IsKeepAlive() const;

private:
  bool ParseRequestLine_(const std::string &line);
  void ParseHeader_(const std::string &line);
  void ParseBody_(const std::string &line);
  void ParsePath_();
  void ParsePost_();
  void ParseFromUrlencoded_();

  static bool UserVerify(const std::string &name, const std::string &pwd,
                         bool isLogin);

  PARSE_STATE state_;
  std::string method_, path_, version_, body_;
  std::unordered_map<std::string, std::string> header_;
  std::unordered_map<std::string, std::string> post_;

  static const std::unordered_set<std::string> DEFAULT_HTML;
  static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
  static int ConvertHex(char ch);
};
