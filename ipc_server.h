#ifndef HACHI_IPC_SERVER_H_
#define HACHI_IPC_SERVER_H_

#include <mutex>
#include <queue>

#include "uv.h"

#include "common.h"
#include "macros.h"

namespace hachi {
class IpcServer {
 public:
  explicit IpcServer(const std::string& pipe_name);
  ~IpcServer();

  void Start();
  void Send(const std::string& msg);
  void Close();

  void SetMessageHandler(MessageHandler handler) { msg_handler_ = handler; }

  void SetErrorHandler(ErrorHandler handler) { error_handler_ = handler; }

  void CallMessageHandler(const std::string& msg) {
    if (msg_handler_ != nullptr) {
      msg_handler_(msg);
    }
  }

  void CallErrorHandler(int code, const std::string& msg) {
    if (error_handler_ != nullptr) {
      error_handler_(code, msg);
    }
  }

  bool IsAlive() { return uv_is_active((uv_handle_t*)server_); }

  void CleanSendQueue();

 private:
  int DoSend();

  friend void IpcServerWorker(void* arg);
  friend void CloseAsyncCallback(uv_async_t* handle);
  friend void OnClientConnection(uv_stream_t* server, int status);
  friend void ReadCallback(uv_stream_t* client,
                           ssize_t nread,
                           const uv_buf_t* buf);
  friend void SendCallback(uv_write_t* req, int status);
  friend void NotifySend(uv_async_t* handle);

  std::string pipe_name_;
  int ipc_type_;
  MessageHandler msg_handler_;
  ErrorHandler error_handler_;

  uv_loop_t* loop_;
  uv_pipe_t* server_;
  uv_pipe_t* client_;
  uv_async_t* send_async_;
  uv_async_t* close_async_;
  uv_thread_t thread_;

  std::queue<uv_buf_t> send_queue_;
  std::mutex queue_mutex_;

  DISALLOW_COPY_AND_ASSIGN(IpcServer);
};
}  // namespace hachi

#endif  // !HACHI_IPC_SERVER_H_
