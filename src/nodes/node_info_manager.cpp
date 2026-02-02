#include "zerolancom/nodes/node_info_manager.hpp"

#include <msgpack.hpp>
#include <zmq.hpp>

#include "zerolancom/sockets/client.hpp"
#include "zerolancom/utils/logger.hpp"
#include "zerolancom/utils/zmq_utils.hpp"

namespace zlc
{

/* ================= Constructor ================= */

NodeInfoManager::NodeInfoManager(const std::string &name, const std::string &ip)
{
  localNodeInfo_.nodeID = generateUUID();
  localNodeInfo_.infoID = 0;
  localNodeInfo_.name = name;
  localNodeInfo_.ip = ip;
}

/* ================= private helpers ================= */

void NodeInfoManager::updateNodeUnlocked(const std::string &nodeID,
                                         const NodeInfo &info)
{
  nodes_info_[nodeID] = info;
  nodes_info_id_[nodeID] = info.infoID;
  nodes_heartbeat_[nodeID] = std::chrono::steady_clock::now();
}

bool NodeInfoManager::checkNodeIDUnlocked(const std::string &nodeID) const
{
  return nodes_info_.find(nodeID) != nodes_info_.end();
}

bool NodeInfoManager::checkNodeInfoIDUnlocked(const std::string &nodeID,
                                              uint32_t infoID) const
{
  auto it = nodes_info_id_.find(nodeID);
  if (it == nodes_info_id_.end())
    return false;
  return it->second == infoID;
}

std::optional<NodeInfo> NodeInfoManager::fetchNodeInfo(const std::string &ip,
                                                       int32_t servicePort)
{
  try
  {
    zlc::info("[NodeInfoManager] Fetching node info from {}:{}", ip, servicePort);
    const std::string service_url = "tcp://" + ip + ":" + std::to_string(servicePort);
    // Create a temporary REQ socket
    NodeInfo info;
    Client::zlcRequest<Empty, NodeInfo>("get_node_info", service_url, Empty{}, info);
    return info;
  }
  catch (const std::exception &e)
  {
    zlc::warn("[NodeInfoManager] Failed to fetch node info from {}:{}: {}", ip,
              servicePort, e.what());
    return std::nullopt;
  }
}

/* ================= public API ================= */

bool NodeInfoManager::checkNodeID(const std::string &nodeID) const
{
  std::shared_lock lock(data_mutex_);
  return checkNodeIDUnlocked(nodeID);
}

bool NodeInfoManager::checkNodeInfoID(const std::string &nodeID, uint32_t infoID) const
{
  std::shared_lock lock(data_mutex_);
  return checkNodeInfoIDUnlocked(nodeID, infoID);
}

void NodeInfoManager::removeNode(const std::string &nodeID)
{
  std::unique_lock lock(data_mutex_);
  nodes_info_.erase(nodeID);
  nodes_info_id_.erase(nodeID);
  nodes_heartbeat_.erase(nodeID);
}

std::vector<SocketInfo>
NodeInfoManager::getPublisherInfo(const std::string &topicName) const
{
  std::shared_lock lock(data_mutex_);

  std::vector<SocketInfo> result;
  for (const auto &[id, node] : nodes_info_)
  {
    for (const auto &t : node.topics)
    {
      if (t.name == topicName)
      {
        result.push_back(t);
      }
    }
  }
  for (const auto &t : localNodeInfo_.topics)
  {
    if (t.name == topicName)
    {
      result.push_back(t);
    }
  }
  return result;
}

const SocketInfo *NodeInfoManager::getServiceInfo(const std::string &serviceName) const
{
  std::shared_lock lock(data_mutex_);

  for (const auto &[id, node] : nodes_info_)
  {
    for (const auto &t : node.services)
    {
      if (t.name == serviceName)
      {
        return &t;
      }
    }
  }
  for (const auto &t : localNodeInfo_.services)
  {
    if (t.name == serviceName)
    {
      return &t;
    }
  }
  return nullptr;
}

void NodeInfoManager::checkHeartbeats()
{
  std::unique_lock lock(data_mutex_);

  auto now = std::chrono::steady_clock::now();
  std::vector<std::string> to_remove;

  for (const auto &[nodeID, last] : nodes_heartbeat_)
  {
    auto duration =
        std::chrono::duration_cast<std::chrono::seconds>(now - last).count();
    if (duration > 2)
    {
      to_remove.push_back(nodeID);
    }
  }

  for (const auto &nodeID : to_remove)
  {
    node_remove_event.trigger(nodes_info_[nodeID]);
    nodes_info_.erase(nodeID);
    nodes_info_id_.erase(nodeID);
    nodes_heartbeat_.erase(nodeID);
    zlc::info("Node {} removed due to heartbeat timeout", nodeID);
  }
}

void NodeInfoManager::processHeartbeat(const HeartbeatMessage &heartbeat,
                                       const std::string &nodeIP)
{
  try
  {
    bool needsFetch = false;
    bool isNew = false;

    {
      std::unique_lock lock(data_mutex_);

      // Update heartbeat timestamp
      nodes_heartbeat_[heartbeat.node_id] = std::chrono::steady_clock::now();

      if (!checkNodeIDUnlocked(heartbeat.node_id))
      {
        // New node - need to fetch full info
        needsFetch = true;
        isNew = true;
      }
      else if (!checkNodeInfoIDUnlocked(heartbeat.node_id,
                                        static_cast<uint32_t>(heartbeat.info_id)))
      {
        // Info changed - need to fetch updated info
        needsFetch = true;
      }
    }

    if (needsFetch)
    {
      auto nodeInfoOpt = fetchNodeInfo(nodeIP, heartbeat.service_port);
      if (nodeInfoOpt.has_value())
      {
        NodeInfo &nodeInfo = nodeInfoOpt.value();

        {
          std::unique_lock lock(data_mutex_);
          updateNodeUnlocked(heartbeat.node_id, nodeInfo);
        }

        if (isNew)
        {
          zlc::info("Node {} added via heartbeat", nodeInfo.name);
          nodeInfo.printNodeInfo();
        }

        node_update_event.trigger(nodeInfo);
      }
    }
  }
  catch (const std::exception &e)
  {
    zlc::error("Error processing heartbeat: {}", e.what());
  }
}

/* ================= Local Node Management ================= */

const UUID &NodeInfoManager::nodeID() const
{
  return localNodeInfo_.nodeID;
}

void NodeInfoManager::setGroupName(const std::string &name)
{
  std::lock_guard<std::mutex> lock(local_mutex_);
  groupName_ = name;
}

void NodeInfoManager::setServicePort(int32_t port)
{
  std::lock_guard<std::mutex> lock(local_mutex_);
  servicePort_ = port;
}

HeartbeatMessage NodeInfoManager::createHeartbeat() const
{
  std::lock_guard<std::mutex> lock(local_mutex_);
  HeartbeatMessage msg;
  msg.zlc_version = {ZLC_VERSION_MAJOR, ZLC_VERSION_MINOR, ZLC_VERSION_PATCH};
  msg.node_id = localNodeInfo_.nodeID;
  msg.info_id = static_cast<int32_t>(localNodeInfo_.infoID);
  msg.service_port = servicePort_;
  msg.group_name = groupName_;
  return msg;
}

NodeInfo NodeInfoManager::getLocalNodeInfo() const
{
  std::lock_guard<std::mutex> lock(local_mutex_);
  return localNodeInfo_;
}

void NodeInfoManager::registerLocalTopic(const std::string &name, uint16_t port)
{
  std::lock_guard<std::mutex> lock(local_mutex_);
  localNodeInfo_.topics.push_back(SocketInfo{name, localNodeInfo_.ip, port});
  ++localNodeInfo_.infoID;
}

void NodeInfoManager::registerLocalService(const std::string &name, uint16_t port)
{
  std::lock_guard<std::mutex> lock(local_mutex_);
  localNodeInfo_.services.push_back(SocketInfo{name, localNodeInfo_.ip, port});
  ++localNodeInfo_.infoID;
}

} // namespace zlc
