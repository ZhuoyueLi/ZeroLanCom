#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <netinet/in.h>
#include <string>
#include <thread>
#include <vector>

#include "zerolancom/nodes/heartbeat_message.hpp"
#include "zerolancom/nodes/node_info_manager.hpp"

namespace zlc
{

class MulticastSender : public Singleton<MulticastSender>
{
public:
  MulticastSender(const std::string &group, int port, const std::string &localIP,
                  const std::string &groupName);
  ~MulticastSender();

  void start();
  void stop();

private:
  void run();
  void sendHeartbeat(const Bytes &msg);
  int sock_;
  sockaddr_in addr_{};
  std::thread thread_;
  std::atomic<bool> running_{false};
  NodeInfoManager *nodeInfoManager_;
  std::string groupName_;
};

class MulticastReceiver : public Singleton<MulticastReceiver>
{
public:
  MulticastReceiver(const std::string &group, int port, const std::string &localIP,
                    const std::string &groupName);
  ~MulticastReceiver();

  void start();
  void stop();

private:
  void run();
  int sock_;
  std::string localIP_;
  std::string groupName_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  NodeInfoManager *nodeInfoManager_;
};

} // namespace zlc
