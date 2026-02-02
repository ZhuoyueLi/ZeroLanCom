#include "zerolancom/serialization/binary_codec.hpp"

#include "zerolancom/utils/exception.hpp"
#include <cstdlib>
#include <cstring>

namespace zlc
{

/* ================= ByteView ================= */

const uint8_t *ByteView::begin() const
{
  return data;
}

const uint8_t *ByteView::end() const
{
  return data + size;
}

/* ================= ByteBuffer ================= */

ByteBuffer::~ByteBuffer()
{
  std::free(data);
}

void ByteBuffer::write(const char *buf, size_t len)
{
  if (size + len > capacity)
  {
    size_t newcap = std::max(capacity * 2, size + len);
    uint8_t *newdata = static_cast<uint8_t *>(std::realloc(data, newcap));
    if (!newdata)
      throw std::bad_alloc();
    data = newdata;
    capacity = newcap;
  }
  std::memcpy(data + size, buf, len);
  size += len;
}

/* ================= Utilities ================= */

std::string decodeServiceHeader(ByteView payload)
{
  constexpr size_t kMaxLen = 1024;

  if (!payload.data || payload.size == 0)
    return {};

  size_t real_len = 0;
  while (real_len < payload.size && real_len < kMaxLen && payload.data[real_len] != 0)
  {
    ++real_len;
  }

  return std::string(reinterpret_cast<const char *>(payload.data), real_len);
}

} // namespace zlc
