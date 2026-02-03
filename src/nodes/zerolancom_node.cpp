#include "zerolancom/nodes/zerolancom_node.hpp"

#include <stdexcept>

namespace zlc
{

ZeroLanComNode::ZeroLanComNode(const std::string &name, const std::string &ip,
                               const std::string &group, int groupPort)
    : ZeroLanComNode(name, ip, group, groupPort, "zlc_default_group_name")
{
}

ZeroLanComNode::ZeroLanComNode(const std::string &name, const std::string &ip,
                               const std::string &group, int groupPort,
                               const std::string &groupName)
{
  zlc::info("[ZeroLanComNode] Initializing ZeroLanComNode '{}' at {}", name, ip);
  zlc::info("[ZeroLanComNode] Using multicast group {}:{} with group name '{}'", group,
            groupPort, groupName);
  ZMQContext::initExternal();
  NodeInfoManager::initExternal(name, ip);
  ServiceManager::initExternal(ip);

  // Set service port in NodeInfoManager before starting multicast
  NodeInfoManager::instance().setServicePort(ServiceManager::instance().service_port);

  MulticastReceiver::initExternal(group, groupPort, ip, groupName);
  MulticastSender::initExternal(group, groupPort, ip, groupName);
  SubscriberManager::initExternal();

  // Register internal get_node_info service
  registerGetNodeInfoService();

  MulticastSender::instance().start();
  MulticastReceiver::instance().start();
  ServiceManager::instance().start();
  SubscriberManager::instance().start();
  running = true;
}

ZeroLanComNode::~ZeroLanComNode()
{
  stop();
}

void ZeroLanComNode::registerGetNodeInfoService()
{
  auto &serviceManager = ServiceManager::instance();
  serviceManager.registerHandler<Empty, NodeInfo>(
      "get_node_info",
      [](const Empty &) -> NodeInfo
      { return NodeInfoManager::instance().getLocalNodeInfo(); });
}

void ZeroLanComNode::stop()
{
  running = false;
  MulticastSender::instance().stop();
  MulticastReceiver::instance().stop();
  ServiceManager::instance().stop();
  SubscriberManager::instance().stop();

  // Destroy in reverse order of initialization, respecting dependencies
  // SubscriberManager subscribes to NodeInfoManager events, so destroy first
  SubscriberManager::destroy();
  ServiceManager::destroy();
  MulticastReceiver::destroy();
  MulticastSender::destroy();
  NodeInfoManager::destroy();
  ZMQContext::destroy();
  // Shutdown logger before destroying singletons to avoid segfault during global dtors
  zlc::info("[ZeroLanComNode] Shutdown complete.");
  Logger::shutdown();
}

bool ZeroLanComNode::isRunning() const
{
  return running;
}

} // namespace zlc
