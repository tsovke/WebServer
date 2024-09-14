#include "server/server.h"
#include <unistd.h>

int main() {
  WebServer sever(1234, 3, 30000, 3306, "user", "password", "webserver", 16, 16,
                  true, 1, 1024);
  sever.Start();
}
