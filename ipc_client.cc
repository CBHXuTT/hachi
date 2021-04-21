#include "ipc_client.h"

#include <assert.h>
#include <iostream>

namespace hachi {

void ClientNotifySend(uv_async_t* handle) {
  IpcClient* ctx = (IpcClient*)uv_handle_get_data((uv_handle_t*)handle);
  if (ctx != nullptr) {
    ctx->DoSend();
  }
}

void ClientSendCallback(uv_write_t* req, int status) {
  write_req_t* wr = (write_req_t*)req;
  IpcClient* ctx = (IpcClient*)uv_handle_get_data((uv_handle_t*)&wr->req);
  free(req);
  if (ctx != nullptr) {
    {
      std::unique_lock<std::mutex> lock(ctx->queue_mutex_);
      if (!ctx->send_queue_.empty()) {
        uv_buf_t buf = ctx->send_queue_.front();
        free(buf.base);
        ctx->send_queue_.pop();
      }
    }
    if (status < 0) {
      if (status == UV_ECANCELED) {
        return;
      }
      ctx->CallErrorHandler(status, uv_err_name(status));
      ctx->Close();
    } else {
      ctx->DoSend();
    }
  }
}

void ClientReadCallback(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
  IpcClient* ctx = (IpcClient*)uv_handle_get_data((uv_handle_t*)client);
  if (ctx != nullptr) {
    if (nread > 0) {
      std::string msg;
      msg.resize(nread);
      std::copy(buf->base, buf->base + buf->len, msg.begin());
      ctx->CallMessageHandler(msg);
      ctx->Send(msg);
    }
    if (nread < 0) {
      if (nread != UV_EOF) {
        // error
        ctx->CallErrorHandler(nread, uv_err_name(nread));
      }
      uv_close((uv_handle_t*)ctx->client_, nullptr);
    }
  }
  free(buf->base);
}

void OnConnection(uv_connect_t* req, int status) {
  IpcClient* ctx = (IpcClient*)uv_handle_get_data((uv_handle_t*)req);
  if (ctx != nullptr) {
    if (status < 0) {
      // error!
      ctx->CallErrorHandler(status, uv_err_name(status));
      return;
    }
    uv_handle_set_data((uv_handle_t*)ctx->client_, (void*)ctx);
    uv_read_start((uv_stream_t*)ctx->client_, alloc_buffer, ClientReadCallback);
  }
}

void ClientCloseAsyncCallback(uv_async_t* handle) {
  IpcClient* ctx = (IpcClient*)uv_handle_get_data((uv_handle_t*)handle);
  if (ctx != nullptr) {
    if (uv_is_active((uv_handle_t*)ctx->client_)) {
      uv_close((uv_handle_t*)ctx->client_, nullptr);
    }
    if (uv_is_active((uv_handle_t*)ctx->connect_)) {
      uv_close((uv_handle_t*)ctx->connect_, nullptr);
    }
    if (uv_is_active((uv_handle_t*)ctx->send_async_)) {
      uv_close((uv_handle_t*)ctx->send_async_, nullptr);
    }
    if (uv_is_active((uv_handle_t*)ctx->close_async_)) {
      uv_close((uv_handle_t*)ctx->close_async_, nullptr);
    }
    uv_fs_unlink(ctx->loop_, nullptr, ctx->pipe_name_.data(), nullptr);
  }
}

void IpcClientWorker(void* arg) {
  IpcClient* ctx = (IpcClient*)arg;

  if (ctx != nullptr) {
    int code = 0;
    code = uv_loop_init(ctx->loop_);
    if (code != 0)
      goto error;

    uv_handle_set_data((uv_handle_t*)ctx->send_async_, (void*)ctx);
    code = uv_async_init(ctx->loop_, ctx->send_async_, ClientNotifySend);

    uv_handle_set_data((uv_handle_t*)ctx->close_async_, (void*)ctx);
    code = uv_async_init(ctx->loop_, ctx->close_async_, ClientCloseAsyncCallback);

    code = uv_pipe_init(ctx->loop_, ctx->client_, ctx->ipc_type_);
    if (code != 0)
      goto error;

    uv_handle_set_data((uv_handle_t*)ctx->connect_, (void*)ctx);
    uv_pipe_connect(ctx->connect_, ctx->client_, ctx->pipe_name_.data(),
                    OnConnection);

    code = uv_run(ctx->loop_, UV_RUN_DEFAULT);
    if (code != 0)
      goto error;

  error:
    uv_loop_close(ctx->loop_);
    if (code != 0) {
      ctx->CallErrorHandler(code, uv_err_name(code));
    }
  }
}

IpcClient::IpcClient(const std::string& pipe_name)
    : pipe_name_(pipe_name), ipc_type_(0) {
  loop_ = new uv_loop_t();
  server_ = new uv_pipe_t();
  client_ = new uv_pipe_t();
  connect_ = new uv_connect_t();
  send_async_ = new uv_async_t();
  close_async_ = new uv_async_t();
}
IpcClient::~IpcClient() {
  delete loop_;
  delete server_;
  delete client_;
  delete connect_;
  delete send_async_;
  delete close_async_;
}

void IpcClient::CleanSendQueue() {
  std::unique_lock<std::mutex> lock(queue_mutex_);
  uv_buf_t buf{};
  while (!send_queue_.empty()) {
    buf = send_queue_.front();
    free(buf.base);
    send_queue_.pop();
  }
}

void IpcClient::Connect() {
  uv_thread_create(&thread_, IpcClientWorker, (void*)this);
  // std::thread th(IpcServerWorker, (void*)this);
  // th.detach();
}

void IpcClient::Send(const std::string& msg) {
  {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    uv_buf_t buf;
    // buf.base = (char*)malloc(msg.size()+sizeof(MessagePrefix));
    // MessagePrefix prefix{ 1,msg.size() };
    // std::copy((char*)&prefix, (char*)&prefix+ sizeof(MessagePrefix),
    // buf.base); std::copy(msg.cbegin(), msg.cend(), buf.base +
    // sizeof(MessagePrefix)); buf.len = msg.size() + sizeof(MessagePrefix);

    buf.base = (char*)malloc(msg.size());
    std::copy(msg.cbegin(), msg.cend(), buf.base);
    buf.len = msg.size();

    send_queue_.emplace(std::move(buf));
  }
  uv_async_send(send_async_);
}

int IpcClient::DoSend() {
  if (client_ == nullptr) {
    return -1;
  }
  uv_buf_t buf{};
  {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (!send_queue_.empty()) {
      buf = send_queue_.front();
    }
  }
  if (buf.len <= 0) {
    return 0;
  }
  write_req_t* req = (write_req_t*)malloc(sizeof(write_req_t));
  uv_handle_set_data((uv_handle_t*)&req->req, (void*)this);
  req->buf = uv_buf_init(buf.base, buf.len);
  return uv_write((uv_write_t*)req, (uv_stream_t*)client_, &req->buf, 1,
                  ClientSendCallback);
}

void IpcClient::Close() {
  CleanSendQueue();
  uv_async_send(close_async_);
  uv_thread_join(&thread_);
}

}  // namespace hachi
