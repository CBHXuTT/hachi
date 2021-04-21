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
#pragma comment(lib,"ws2_32.lib")
#pragma comment (lib,"Advapi32.lib")
#pragma comment (lib,"Iphlpapi.lib")
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "userenv.lib")
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