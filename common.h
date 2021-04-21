#ifndef HACHI_COMMON_H_
#define HACHI_COMMON_H_

#include <stdlib.h>
#include <functional>
#include <string>

#include "uv.h"

namespace hachi {

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define container_of(ptr, type, member) \
  ((type*)((char*)(ptr)-offsetof(type, member)))

using MessageHandler = std::function<void(std::string msg)>;
using ErrorHandler = std::function<void(int code, std::string msg)>;

typedef struct {
  uv_write_t req;
  uv_buf_t buf;
} write_req_t;

void free_write_req(uv_write_t* req);
void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);

struct Message {
  uint8_t type;
  std::string data;
};

struct MessagePrefix {
  uint64_t type;
  uint64_t size;
};

}  // namespace hachi

#endif  // !HACHI_COMMON_H_
