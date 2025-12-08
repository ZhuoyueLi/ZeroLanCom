#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <optional>
#include <iostream>
#include <shared_mutex>
#include "nodes/node_info.hpp"
#include "utils/logger.hpp"


namespace zlc {

class NodeInfoManager {
private:
    mutable std::shared_mutex data_mutex_;
    std::unordered_map<std::string, NodeInfo> nodes_info;
    std::unordered_map<std::string, uint32_t> nodes_info_id;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> nodes_heartbeat;
    mutable std::shared_mutex callback_mutex_;
    std::vector<std::function<void(const NodeInfo&)>> update_callback_;
    
    void UpdateNodeUnlocked(const std::string& nodeID, const NodeInfo& info) {
        nodes_info[nodeID] = info;
        nodes_info_id[nodeID] = info.infoID;
        nodes_heartbeat[nodeID] = std::chrono::steady_clock::now();
    }

    bool checkNodeIDUnlocked(const std::string& nodeID) const {
        return nodes_info.find(nodeID) != nodes_info.end();
    }

    bool checkNodeInfoIDUnlocked(const std::string& nodeID, uint32_t infoID) const {
        auto it = nodes_info_id.find(nodeID);
        if (it == nodes_info_id.end()) return false;
        return it->second == infoID;
    }


public:

    void registerUpdateCallback(const std::function<void(const NodeInfo&)>& callback)
    {
        std::unique_lock lock(callback_mutex_);
        update_callback_.push_back(callback);
    }

    bool checkNodeID(const std::string& nodeID) const {
        std::shared_lock lock(data_mutex_);
        return checkNodeIDUnlocked(nodeID);
    }

    bool checkNodeInfoID(const std::string& nodeID, uint32_t infoID) const {
        std::shared_lock lock(data_mutex_);
        return checkNodeInfoIDUnlocked(nodeID, infoID);
    }

    void removeNode(const std::string& nodeID) {
        std::unique_lock lock(data_mutex_);
        nodes_info.erase(nodeID);
        nodes_info_id.erase(nodeID);
        nodes_heartbeat.erase(nodeID);
    }

    std::vector<SocketInfo> getPublisherInfo(const std::string& topicName) const {
        std::shared_lock lock(data_mutex_);
        std::vector<SocketInfo> result;
        for (auto& [id, node] : nodes_info) {
            for (auto& t : node.topics) {
                if (t.name == topicName)
                    result.push_back(t);
            }
        }
        return result;
    }

    const SocketInfo* getServiceInfo(const std::string& serviceName) const {
        std::shared_lock lock(data_mutex_);
        for (auto& [id, node] : nodes_info) {
            for (auto& t : node.services) {
                if (t.name == serviceName)
                    return &t;
            }
        }
        return nullptr;
    }

    void checkHeartbeats() {
        std::unique_lock lock(data_mutex_);
        auto now = std::chrono::steady_clock::now();
        std::vector<std::string> to_remove;
        for (auto& [nodeID, last_heartbeat] : nodes_heartbeat) {
            // Here we would check the timestamp of the last heartbeat
            // For simplicity, we assume if the IP is empty, the node is dead
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_heartbeat).count();
            if (duration > 2) {
                to_remove.push_back(nodeID);
            }
        }
        for (const auto& nodeID : to_remove) {
            nodes_info.erase(nodeID);
            nodes_info_id.erase(nodeID);
            nodes_heartbeat.erase(nodeID);
            LOG_INFO("Node {} removed due to heartbeat timeout", nodeID);
        }
    }

    void processHeartbeat(const NodeInfo& nodeInfo) {
        try {
            bool is_new = false;
            bool info_changed = false;
            {
                std::unique_lock lock(data_mutex_);

                if (!checkNodeIDUnlocked(nodeInfo.nodeID)) {
                    UpdateNodeUnlocked(nodeInfo.nodeID, nodeInfo);
                    is_new = true;
                }
                else if (!checkNodeInfoIDUnlocked(nodeInfo.nodeID, nodeInfo.infoID)) {
                    UpdateNodeUnlocked(nodeInfo.nodeID, nodeInfo);
                    info_changed = true;
                }
                else {
                    // Update only the heartbeat timestamp
                    nodes_heartbeat[nodeInfo.nodeID] = std::chrono::steady_clock::now();
                }
            }
            // spdlog logging outside lock since it has its own internal locking
            if (is_new) {
                LOG_INFO("Node {} added via heartbeat", nodeInfo.name);
                nodeInfo.printNodeInfo();
                std::shared_lock lock(callback_mutex_);
                for (const auto& callback : update_callback_) {
                    callback(nodeInfo);
                }
            }
            else if (info_changed) {
                std::shared_lock lock(callback_mutex_);
                for (const auto& callback : update_callback_) {
                    callback(nodeInfo);
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error processing heartbeat: " << e.what() << "\n";
        }
    }

};

} // namespace zlc