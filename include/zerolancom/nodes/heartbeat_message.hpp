#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "zerolancom/serialization/binary_codec.hpp"
#include "zerolancom/utils/message.hpp"

namespace zlc
{

// Protocol version constants
constexpr int32_t ZLC_VERSION_MAJOR = 2;
constexpr int32_t ZLC_VERSION_MINOR = 0;
constexpr int32_t ZLC_VERSION_PATCH = 2;

/**
 * @brief Lightweight heartbeat message for node discovery.
 *
 * This message is sent periodically via multicast to announce node presence.
 * When a receiver detects a new node or info_id change, it fetches full
 * NodeInfo via the service port.
 *
 * Binary format (network byte order / big-endian):
 *   - zlc_version: 3 x int32 (12 bytes)
 *   - node_id: 36 bytes fixed string (UUID)
 *   - info_id: int32 (4 bytes)
 *   - service_port: int32 (4 bytes)
 *   - group_name: remaining bytes (variable length string)
 *
 * Total fixed size: 56 bytes + group_name length
 */
struct HeartbeatMessage
{
  std::array<int32_t, 3> zlc_version; // {major, minor, patch}
  UUID node_id;                       // 36-char UUID string
  int32_t info_id;
  int32_t service_port;
  std::string group_name;

  /**
   * @brief Serialize heartbeat message to bytes (network byte order).
   */
  Bytes encode() const;

  /**
   * @brief Deserialize heartbeat message from bytes.
   * @param data Raw bytes received from multicast
   * @param size Size of the data buffer
   * @return Decoded HeartbeatMessage
   * @throws std::runtime_error if data is too short or malformed
   */
  static HeartbeatMessage decode(const uint8_t *data, size_t size);
};

} // namespace zlc
