#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <zmq.hpp>

#include <atomic>
#include <thread>

#include "zerolancom/serialization/serializer.hpp"
#include "zerolancom/utils/logger.hpp"
#include "zerolancom/utils/request_result.hpp"
#include "zerolancom/utils/zmq_utils.hpp"

namespace zlc
{

using ServiceCallback = std::function<Bytes(const ByteView &payload)>;

/**
 * @brief ServiceManager handles incoming RPC service requests.
 *
 * Design notes:
 * - Uses ZMQ REP socket for request handling with dedicated polling thread.
 * - Template registerHandler functions must remain header-only.
 * - Non-template functions are implemented in service_manager.cpp.
 */
class ServiceManager : public Singleton<ServiceManager>
{
public:
  int service_port{0};

  explicit ServiceManager(const std::string &ip);
  ~ServiceManager();

  void start();
  void stop();

  /**
   * @brief Register a request-response handler.
   */
  template <typename RequestType, typename ResponseType>
  void registerHandler(const std::string &name,
                       const std::function<ResponseType(const RequestType &)> &func)
  {
    handlers_[name] = [func](const ByteView &payload) -> Bytes
    {
      RequestType req;
      decode(payload, req);
      ResponseType resp = func(req);
      ByteBuffer out;
      encode(resp, out);
      return Bytes(out.data, out.data + out.size);
    };
  }

  template <typename RequestType, typename ResponseType, typename ClassT>
  void registerHandler(const std::string &name,
                       ResponseType (ClassT::*func)(const RequestType &),
                       ClassT *instance)
  {
    handlers_[name] = [instance, func](const ByteView &payload) -> Bytes
    {
      RequestType req;
      decode(payload, req);
      ResponseType resp = (instance->*func)(req);
      ByteBuffer out;
      encode(resp, out);
      return Bytes(out.data, out.data + out.size);
    };
  }

  void handleRequest(const std::string &service_name, const ByteView &payload,
                     Response &response);

  void clearHandlers();
  void removeHandler(const std::string &name);

  // Non-copyable, movable
  ServiceManager(const ServiceManager &) = delete;
  ServiceManager &operator=(const ServiceManager &) = delete;
  ServiceManager(ServiceManager &&) = default;
  ServiceManager &operator=(ServiceManager &&) = default;

private:
  // Poll once for incoming service requests
  void pollOnce();

private:
  void run();

private:
  std::unordered_map<std::string, std::function<Bytes(const ByteView &)>> handlers_;

  ZMQSocket *res_socket_;
  static constexpr int SOCKET_TIMEOUT_MS = 100;

  std::thread thread_;
  std::atomic<bool> running_{false};
};

} // namespace zlc
