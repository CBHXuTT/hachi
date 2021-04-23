#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <thread>

#include "uv.h"

#include "common.h"
#include "ipc_server.h"

#ifdef _WIN32
#define PIPENAME "\\\\?\\pipe\\worklinkshot.sock"
#else
#define PIPENAME "/worklinkshot/shot.sock"
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
    //server->Send("hello");
    alive = server->IsAlive();
  }

  return 0;
}