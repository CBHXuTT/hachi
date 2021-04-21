#ifndef HACHI_IPC_SERVER_H_
#define HACHI_IPC_SERVER_H_

#include <mutex>
#include <queue>

#include "uv.h"

#include "common.h"
#include "macros.h"

namespace hachi {
class IpcClient {
 public:
  explicit IpcClient(const std::string& pipe_name);
  ~IpcClient();

  void Connect();
  void Send(const std::string& msg);
  void Close();

  bool IsAlive() { return uv_is_active((uv_handle_t*)client_); }

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

  void CleanSendQueue();

 private:
  int DoSend();

  friend void IpcClientWorker(void* arg);
  friend void ClientCloseAsyncCallback(uv_async_t* handle);
  friend void OnConnection(uv_connect_t* req, int status);
  friend void ClientReadCallback(uv_stream_t* client,
                           ssize_t nread,
                           const uv_buf_t* buf);
  friend void ClientSendCallback(uv_write_t* req, int status);
  friend void ClientNotifySend(uv_async_t* handle);

  std::string pipe_name_;
  int ipc_type_;
  MessageHandler msg_handler_;
  ErrorHandler error_handler_;

  uv_loop_t* loop_;
  uv_pipe_t* server_;
  uv_pipe_t* client_;
  uv_connect_t* connect_;
  uv_async_t* send_async_;
  uv_async_t* close_async_;
  uv_thread_t thread_;

  std::queue<uv_buf_t> send_queue_;
  std::mutex queue_mutex_;

  DISALLOW_COPY_AND_ASSIGN(IpcClient);
};
}  // namespace hachi

#endif  // !HACHI_IPC_SERVER_H_
