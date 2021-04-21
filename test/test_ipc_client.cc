#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <thread>

#include "uv.h"

#include "common.h"
#include "ipc_client.h"

#ifdef _WIN32
#define PIPENAME "\\\\?\\pipe\\echo.sock"
#else
#define PIPENAME "/tmp/echo.sock"
#endif



int main() {
  hachi::IpcClient client(PIPENAME);

    client.SetMessageHandler([](std::string msg) {
    std::cout << "Message:" << msg << std::endl;
    });

  client.SetErrorHandler([](int code, std::string msg) {
    std::cout << "Error: code(" << code << ") " << msg << std::endl;
    });

  client.Connect();

  bool once = true;
  bool alive = true;

  while (alive) {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    if (once) {
      //client.Close();
      once = false;
      //client.Send("hello");
    }
    client.Send("hello");
    alive = client.IsAlive();
  }
    
  return 0;
}