#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <thread>

#include "uv.h"

#include "common.h"
#include "ipc_server.h"

#ifdef _WIN32
#define PIPENAME "\\\\?\\pipe\\echo.sock"
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "userenv.lib")
#else
#define PIPENAME "/tmp/echo.sock"
#endif

int main() {
  hachi::IpcServer* server = new hachi::IpcServer(PIPENAME);
  server->SetMessageHandler(
      [](std::string msg) { std::cout << "Message:" << msg << std::endl; });

  server->SetErrorHandler([](int code, std::string msg) {
    std::cout << "Error: code(" << code << ") " << msg << std::endl;
  });

  server->Start();

  bool once = true;
  bool alive = true;

  while (alive) {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    if (once) {
      // server->Close();
      once = false;
      // server->Send("hello");
    }
    server->Send("hello");
    alive = server->IsAlive();
  }

  return 0;
}