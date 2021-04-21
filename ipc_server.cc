#include "ipc_server.h"

#include <assert.h>
#include <iostream>

namespace hachi {

void NotifySend(uv_async_t* handle) {
  IpcServer* ctx = (IpcServer*)uv_handle_get_data((uv_handle_t*)handle);
  if (ctx != nullptr) {
    ctx->DoSend();
  }
}

void SendCallback(uv_write_t* req, int status) {
  write_req_t* wr = (write_req_t*)req;
  IpcServer* ctx = (IpcServer*)uv_handle_get_data((uv_handle_t*)&wr->req);
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

void ReadCallback(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
  IpcServer* ctx = (IpcServer*)uv_handle_get_data((uv_handle_t*)client);
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

void OnClientConnection(uv_stream_t* server, int status) {
  IpcServer* ctx = (IpcServer*)uv_handle_get_data((uv_handle_t*)server);
  if (ctx != nullptr) {
    if (status == -1) {
      // error!
      ctx->CallErrorHandler(status, uv_err_name(status));
      return;
    }
    uv_pipe_t* client = ctx->client_;
    uv_pipe_init(ctx->loop_, client, 0);
    if (uv_accept(server, (uv_stream_t*)client) == 0) {
      uv_handle_set_data((uv_handle_t*)client, (void*)ctx);
      uv_read_start((uv_stream_t*)client, alloc_buffer, ReadCallback);
    } else {
      uv_close((uv_handle_t*)client, nullptr);
    }
  }
}

void CloseAsyncCallback(uv_async_t* handle) {
  IpcServer* ctx = (IpcServer*)uv_handle_get_data((uv_handle_t*)handle);
  if (ctx != nullptr) {
    if (uv_is_active((uv_handle_t*)ctx->client_)) {
      uv_close((uv_handle_t*)ctx->client_, nullptr);
    }
    if (uv_is_active((uv_handle_t*)ctx->server_)) {
      uv_close((uv_handle_t*)ctx->server_, nullptr);
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

void IpcServerWorker(void* arg) {
  IpcServer* ctx = (IpcServer*)arg;

  if (ctx != nullptr) {
    int code = 0;
    code = uv_loop_init(ctx->loop_);
    if (code != 0) goto error;

    uv_handle_set_data((uv_handle_t*)ctx->send_async_, (void*)ctx);
    code = uv_async_init(ctx->loop_, ctx->send_async_, NotifySend);

    uv_handle_set_data((uv_handle_t*)ctx->close_async_, (void*)ctx);
    code = uv_async_init(ctx->loop_, ctx->close_async_, CloseAsyncCallback);

    code = uv_pipe_init(ctx->loop_, ctx->server_, ctx->ipc_type_);
    if (code != 0)
      goto error;

    uv_handle_set_data((uv_handle_t*)ctx->server_, (void*)ctx);

    code = uv_pipe_bind(ctx->server_, ctx->pipe_name_.c_str());
    if (code != 0)
      goto error;

    code = uv_listen((uv_stream_t*)ctx->server_, 128, OnClientConnection);
    if (code != 0)
      goto error;

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

IpcServer::IpcServer(const std::string& pipe_name)
    : pipe_name_(pipe_name), ipc_type_(0) {
  loop_ = new uv_loop_t();
  server_ = new uv_pipe_t();
  client_ = new uv_pipe_t();
  send_async_ = new uv_async_t();
  close_async_ = new uv_async_t();
}
IpcServer::~IpcServer() {
  delete loop_;
  delete server_;
  delete client_;
  delete send_async_;
  delete close_async_;
}

void IpcServer::CleanSendQueue() {
  std::unique_lock<std::mutex> lock(queue_mutex_);
  uv_buf_t buf{};
  while (!send_queue_.empty()) {
    buf = send_queue_.front();
    free(buf.base);
    send_queue_.pop();
  }
}

void IpcServer::Start() {
  uv_thread_create(&thread_, IpcServerWorker, (void*)this);
  // std::thread th(IpcServerWorker, (void*)this);
  // th.detach();
}

void IpcServer::Send(const std::string& msg) {
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

int IpcServer::DoSend() {
  if (client_ == nullptr) {
    return -1;
  }
  // if (!uv_is_active((uv_handle_t*)client_)) {
  //  uv_handle_set_data((uv_handle_t*)client_, (void*)this);
  //  uv_read_start((uv_stream_t*)client_, alloc_buffer, ReadCallback);
  //}
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
                  SendCallback);
}

void IpcServer::Close() {
  CleanSendQueue();
  uv_async_send(close_async_);
  uv_thread_join(&thread_);
}

}  // namespace hachi
