#include "common.h"

namespace hachi {
  
void free_write_req(uv_write_t* req) {
  write_req_t* wr = (write_req_t*)req;
  free(wr->buf.base);
  free(wr);
}

void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = (char*)malloc(suggested_size);
  buf->len = suggested_size;
}
}  // namespace hachi