#include "zerolancom/zerolancom.hpp"
#include "zerolancom/nodes/zerolancom_node.hpp"

#include <chrono>
#include <thread>

namespace zlc
{

void init(const std::string &node_name, const std::string &ip_address,
          const std::string &group, int groupPort, const std::string &groupName)
{
  Logger::init(false);
  Logger::setLevel(LogLevel::INFO);
  ZeroLanComNode::initManaged(node_name, ip_address, group, groupPort, groupName);
}

void shutdown()
{
  ZeroLanComNode::destroy();
}

void sleep(int ms)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void spin()
{
  auto &node = ZeroLanComNode::instance();

  try
  {
    while (node.isRunning())
    {
      sleep(100);
    }
  }
  catch (const std::exception &e)
  {
    zlc::error("Exception caught in spin: {}", e.what());
  }

  node.stop();
}

void waitForService(const std::string &service_name, int max_wait_ms,
                    int check_interval_ms)
{
  int waited_ms = 0;

  while (waited_ms < max_wait_ms)
  {
    auto serviceInfoPtr = NodeInfoManager::instance().getServiceInfo(service_name);
    if (serviceInfoPtr != nullptr)
    {
      zlc::info("[Client] Service '{}' is now available.", service_name);
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
    waited_ms += check_interval_ms;
  }
  zlc::warn("[Client] Timeout waiting for service '{}'", service_name);
}
} // namespace zlc
