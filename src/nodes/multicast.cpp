#include "zerolancom/nodes/multicast.hpp"

#include <arpa/inet.h>
#include <unistd.h>

#include "zerolancom/utils/exception.hpp"
#include "zerolancom/utils/logger.hpp"
#include "zerolancom/utils/zmq_utils.hpp"

namespace zlc
{

/* ================= MulticastSender ================= */

MulticastSender::MulticastSender(const std::string &group, int port,
                                 const std::string &localIP,
                                 const std::string &groupName)
    : groupName_(groupName)
{
  sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  int ttl = 1;
  setsockopt(sock_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

  in_addr local{};
  local.s_addr = inet_addr(localIP.c_str());
  setsockopt(sock_, IPPROTO_IP, IP_MULTICAST_IF, &local, sizeof(local));

  addr_.sin_family = AF_INET;
  addr_.sin_port = htons(port);
  addr_.sin_addr.s_addr = inet_addr(group.c_str());
  nodeInfoManager_ = NodeInfoManager::instancePtr();
  nodeInfoManager_->setGroupName(groupName);
}

MulticastSender::~MulticastSender()
{
  stop();
  if (sock_ >= 0)
  {
    close(sock_);
  }
}

void MulticastSender::start()
{
  heartbeat_task_ = std::make_unique<PeriodicTask>(
      [this]()
      {
        auto msg = nodeInfoManager_->createHeartbeat();
        auto bytes = msg.encode();
        this->sendHeartbeat(bytes);
      },
      1000, ThreadPool::instance()); // Send every 1000ms

  heartbeat_task_->start();
}

void MulticastSender::stop()
{
  if (heartbeat_task_)
  {
    heartbeat_task_->stop();
  }
}

void MulticastSender::sendHeartbeat(const Bytes &msg)
{
  sendto(sock_, msg.data(), msg.size(), 0, reinterpret_cast<sockaddr *>(&addr_),
         sizeof(addr_));
}

/* ================= MulticastReceiver ================= */

MulticastReceiver::MulticastReceiver(const std::string &group, int port,
                                     const std::string &localIP,
                                     const std::string &groupName)
    : localIP_(localIP), groupName_(groupName)
{
  sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  int reuse = 1;
  setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  setsockopt(sock_, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  bind(sock_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));

  ip_mreq mreq{};
  mreq.imr_multiaddr.s_addr = inet_addr(group.c_str());
  mreq.imr_interface.s_addr = inet_addr(localIP.c_str());
  setsockopt(sock_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

  nodeInfoManager_ = NodeInfoManager::instancePtr();
}

MulticastReceiver::~MulticastReceiver()
{
  stop();
  if (sock_ >= 0)
  {
    close(sock_);
  }
}

void MulticastReceiver::start()
{
  receive_task_ = std::make_unique<PeriodicTask>(
      [this]()
      {
        Bytes buf(1024);
        sockaddr_in src{};
        socklen_t slen = sizeof(src);

        int n = recvfrom(sock_, buf.data(), static_cast<int>(buf.size()), 0,
                         reinterpret_cast<sockaddr *>(&src), &slen);

        if (n <= 0)
          return;

        std::string nodeIP = inet_ntoa(src.sin_addr);

        // Check if in same subnet
        if (!isInSameSubnet(localIP_, nodeIP))
          return;

        try
        {
          HeartbeatMessage heartbeat =
              HeartbeatMessage::decode(buf.data(), static_cast<size_t>(n));

          // Filter by group name
          if (heartbeat.group_name != groupName_)
            return;

          // Ignore own heartbeat
          if (heartbeat.node_id == nodeInfoManager_->nodeID())
            return;

          nodeInfoManager_->processHeartbeat(heartbeat, nodeIP);
          nodeInfoManager_->checkHeartbeats();
        }
        catch (const std::exception &e)
        {
          warn("[MulticastReceiver] Failed to decode heartbeat from {}: {}", nodeIP,
               e.what());
        }
      },
      100, ThreadPool::instance()); // Poll every 100ms

  receive_task_->start();
}

void MulticastReceiver::stop()
{
  if (receive_task_)
  {
    receive_task_->stop();
  }
}

} // namespace zlc
