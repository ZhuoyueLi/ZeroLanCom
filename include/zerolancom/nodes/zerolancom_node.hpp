#pragma once

#include <string>

#include "zerolancom/utils/logger.hpp"
#include "zerolancom/utils/singleton.hpp"
#include "zerolancom/utils/thread_pool.hpp"

#include "zerolancom/nodes/multicast.hpp"
#include "zerolancom/nodes/node_info_manager.hpp"
#include "zerolancom/sockets/service_manager.hpp"
#include "zerolancom/sockets/subscriber_manager.hpp"

namespace zlc
{

class ZeroLanComNode : public Singleton<ZeroLanComNode>
{
public:
  ZeroLanComNode(const std::string &name, const std::string &ip,
                 const std::string &group, int groupPort);
  ZeroLanComNode(const std::string &name, const std::string &ip,
                 const std::string &group, int groupPort, const std::string &groupName);
  ~ZeroLanComNode();

  void stop();
  bool isRunning() const;

private:
  void registerGetNodeInfoService();
  bool running;
};

} // namespace zlc
